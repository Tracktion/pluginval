/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "AcceptanceTest.h"
#include "../TestUtilities.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace acceptance
{

//==============================================================================
namespace
{
    juce::String archString()
    {
       #if defined (__aarch64__) || defined (_M_ARM64)
        return "arm64";
       #elif defined (__x86_64__) || defined (_M_X64)
        return "x86_64";
       #else
        return "unknown";
       #endif
    }

    //==============================================================================
    /** A fixed-tempo transport whose position advances with the render. Constructed
        from the config's playhead block and pointed at the plugin for the duration
        of the render; setSamplePosition() is called before each processBlock. */
    class FixedPlayHead : public juce::AudioPlayHead
    {
    public:
        FixedPlayHead (const TestConfig::PlayheadConfig& config, double sr)
            : cfg (config), sampleRate (sr)
        {
        }

        void setSamplePosition (juce::int64 sample) noexcept   { currentSample = sample; }

        juce::Optional<PositionInfo> getPosition() const override
        {
            const double seconds = (double) currentSample / sampleRate;

            // PPQ is measured in quarter notes; a bar is numerator * (4/denominator) of them.
            const double quarterNotesPerBar = cfg.timeSigNumerator * 4.0 / cfg.timeSigDenominator;
            const double ppq = cfg.startPpq + seconds * (cfg.bpm / 60.0);

            PositionInfo info;
            info.setBpm (cfg.bpm);
            info.setTimeSignature (TimeSignature { cfg.timeSigNumerator, cfg.timeSigDenominator });
            info.setTimeInSamples (currentSample);
            info.setTimeInSeconds (seconds);
            info.setPpqPosition (ppq);
            info.setPpqPositionOfLastBarStart (std::floor (ppq / quarterNotesPerBar) * quarterNotesPerBar);
            info.setIsPlaying (true);
            return info;
        }

    private:
        const TestConfig::PlayheadConfig cfg;
        const double sampleRate;
        juce::int64 currentSample = 0;
    };

    //==============================================================================
    std::unique_ptr<juce::AudioPluginInstance> loadPlugin (juce::AudioPluginFormatManager& formatManager,
                                                           const juce::String& pathOrID,
                                                           double sampleRate, int blockSize)
    {
        juce::KnownPluginList list;
        juce::OwnedArray<juce::PluginDescription> found;
        list.scanAndAddDragAndDroppedFiles (formatManager, juce::StringArray (pathOrID), found);

        if (found.isEmpty())
            throw std::runtime_error (("no plugin found at: " + pathOrID
                + " (missing/damaged binary, an incompatible format, or an AU not registered with macOS)").toStdString());

        juce::String error;
        auto instance = std::unique_ptr<juce::AudioPluginInstance> (
            formatManager.createPluginInstance (*found.getFirst(), sampleRate, blockSize, error));

        if (instance == nullptr)
            throw std::runtime_error (("failed to create plugin instance: " + error).toStdString());

        return instance;
    }

    //==============================================================================
    void applyState (juce::AudioPluginInstance& instance, const TestConfig& config)
    {
        // Binary state first (the full plugin state), then the parameter map as
        // overrides (see the precedence rule in the spec).
        if (const auto stateFile = config.getStateFile(); stateFile != juce::File())
        {
            if (! stateFile.existsAsFile())
                throw std::runtime_error (("state.file not found: " + stateFile.getFullPathName()).toStdString());

            juce::MemoryBlock state;
            stateFile.loadFileAsData (state);
            callSetStateInformationOnMessageThreadIfVST3 (instance, state);
        }

        for (const auto& [key, value] : config.stateParameters)
        {
            const juce::String name (key);
            juce::AudioProcessorParameter* match = nullptr;

            if (name.containsOnly ("0123456789") && name.isNotEmpty())
            {
                match = instance.getParameters()[name.getIntValue()];
            }
            else
            {
                // Match the display name (case-insensitively), or the JUCE paramID
                // when the host exposes parameters as AudioProcessorParameterWithID.
                for (auto* p : instance.getParameters())
                {
                    if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                        if (withID->paramID.equalsIgnoreCase (name))
                        {
                            match = p;
                            break;
                        }

                    if (p->getName (512).equalsIgnoreCase (name))
                    {
                        match = p;
                        break;
                    }
                }
            }

            if (match == nullptr)
                throw std::runtime_error (("state.parameters: no parameter named or indexed \"" + name + "\"").toStdString());

            match->setValueNotifyingHost ((float) value);
        }
    }

    //==============================================================================
    juce::AudioBuffer<float> readWav (juce::AudioFormatManager& formatManager, const juce::File& file)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

        if (reader == nullptr)
            throw std::runtime_error (("could not read audio file: " + file.getFullPathName()).toStdString());

        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        return buffer;
    }

    void writeFloatWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());

        if (stream == nullptr)
            throw std::runtime_error (("could not open for writing: " + file.getFullPathName()).toStdString());

        juce::WavAudioFormat wav;
        const auto options = juce::AudioFormatWriterOptions()
                                 .withSampleRate (sampleRate)
                                 .withNumChannels (buffer.getNumChannels())
                                 .withBitsPerSample (32)
                                 .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream, options));

        if (writer == nullptr)
            throw std::runtime_error (("could not create WAV writer for: " + file.getFullPathName()).toStdString());

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    //==============================================================================
    /** Builds a single MidiBuffer with events at absolute sample positions. */
    juce::MidiBuffer loadMidi (const juce::File& file, double sampleRate)
    {
        juce::FileInputStream stream (file);

        if (! stream.openedOk())
            throw std::runtime_error (("could not read MIDI file: " + file.getFullPathName()).toStdString());

        juce::MidiFile midiFile;

        if (! midiFile.readFrom (stream))
            throw std::runtime_error (("could not parse MIDI file: " + file.getFullPathName()).toStdString());

        midiFile.convertTimestampTicksToSeconds();

        juce::MidiBuffer buffer;

        for (int t = 0; t < midiFile.getNumTracks(); ++t)
            if (const auto* track = midiFile.getTrack (t))
                for (const auto* event : *track)
                {
                    const auto sample = (int) std::lround (event->message.getTimeStamp() * sampleRate);
                    buffer.addEvent (event->message, sample);
                }

        return buffer;
    }

    //==============================================================================
    juce::String configHash (const TestConfig& config)
    {
        auto j = nlohmann::json {};
        to_json (j, config);
        j.erase ("reference");   // the hash must be independent of where the reference lives

        // A small, stable (cross-platform) FNV-1a 64-bit hash. The hash is only
        // used for an informational "stale reference" warning, so juce_cryptography
        // (MD5) isn't worth pulling in.
        const auto dump = j.dump();
        std::uint64_t hash = 1469598103934665603ull;

        for (const auto c : dump)
        {
            hash ^= (std::uint64_t) (unsigned char) c;
            hash *= 1099511628211ull;
        }

        return juce::String::toHexString ((juce::int64) hash);
    }
}

//==============================================================================
RenderedAudio renderPlugin (const TestConfig& config)
{
    const auto sampleRate = config.sampleRate;
    const auto blockSize = juce::jmax (1, config.blockSize);

    juce::AudioPluginFormatManager formatManager;
   #if JUCE_VERSION >= 0x08000B
    juce::addDefaultFormatsToManager (formatManager);
   #else
    formatManager.addDefaultFormats();
   #endif

    auto instance = loadPlugin (formatManager, config.getPluginPathOrID(), sampleRate, blockSize);
    const auto description = instance->getPluginDescription();

    // 1. State (file then parameter overrides), applied before prepareToPlay.
    applyState (*instance, config);

    // 2. Input audio / MIDI (or silence).
    juce::AudioFormatManager audioFormats;
    audioFormats.registerBasicFormats();

    juce::AudioBuffer<float> inputAudio;
    if (const auto audioFile = config.getInputAudioFile(); audioFile != juce::File())
    {
        if (! audioFile.existsAsFile())
            throw std::runtime_error (("input.audio not found: " + audioFile.getFullPathName()).toStdString());

        inputAudio = readWav (audioFormats, audioFile);
    }

    juce::MidiBuffer midi;
    if (const auto midiFile = config.getInputMidiFile(); midiFile != juce::File())
    {
        if (! midiFile.existsAsFile())
            throw std::runtime_error (("input.midi not found: " + midiFile.getFullPathName()).toStdString());

        midi = loadMidi (midiFile, sampleRate);
    }

    // 3. Render length.
    int numSamples = 0;
    if (config.renderDuration)
        numSamples = (int) std::lround (*config.renderDuration * sampleRate);
    else if (inputAudio.getNumSamples() > 0)
        numSamples = inputAudio.getNumSamples();

    if (numSamples <= 0)
        throw std::runtime_error ("render_duration is required when there is no input.audio");

    // 4. Optional fixed transport for time-dependent plugins.
    std::unique_ptr<FixedPlayHead> playHead;
    if (config.playhead)
    {
        playHead = std::make_unique<FixedPlayHead> (*config.playhead, sampleRate);
        instance->setPlayHead (playHead.get());
    }

    // 5. Prepare and render block by block.
    callPrepareToPlayOnMessageThreadIfVST3 (*instance, sampleRate, blockSize);

    const int numInputChannels = instance->getTotalNumInputChannels();
    const int numOutputChannels = juce::jmax (1, instance->getTotalNumOutputChannels());
    const int channelsRequired = juce::jmax (numInputChannels, numOutputChannels);

    juce::AudioBuffer<float> output (numOutputChannels, numSamples);
    output.clear();

    juce::AudioBuffer<float> block (channelsRequired, blockSize);

    for (int pos = 0; pos < numSamples; pos += blockSize)
    {
        const int thisBlock = juce::jmin (blockSize, numSamples - pos);

        if (playHead != nullptr)
            playHead->setSamplePosition (pos);

        block.clear();

        // Copy the input audio slice into the block (silence past its end).
        for (int c = 0; c < juce::jmin (numInputChannels, inputAudio.getNumChannels()); ++c)
        {
            const int available = juce::jmax (0, juce::jmin (thisBlock, inputAudio.getNumSamples() - pos));
            if (available > 0)
                block.copyFrom (c, 0, inputAudio, c, pos, available);
        }

        juce::MidiBuffer blockMidi;
        blockMidi.addEvents (midi, pos, thisBlock, -pos);

        juce::AudioBuffer<float> proc (block.getArrayOfWritePointers(), channelsRequired, thisBlock);
        instance->processBlock (proc, blockMidi);

        for (int c = 0; c < numOutputChannels; ++c)
            output.copyFrom (c, pos, proc, c, 0, thisBlock);
    }

    instance->setPlayHead (nullptr);   // playHead is about to be destroyed
    callReleaseResourcesOnMessageThreadIfVST3 (*instance);
    instance.reset();

    return { std::move (output), sampleRate, blockSize, description };
}

//==============================================================================
TestResult runTest (const TestConfig& config)
{
    const auto name = config.getName();

    try
    {
        const auto rendered = renderPlugin (config);
        const auto referenceFile = config.getReferenceFile();

        TestResult result;
        result.name = name;
        result.referenceFile = referenceFile;

        // Record mode: no reference yet -> write the float WAV + JSON sidecar.
        if (! referenceFile.existsAsFile())
        {
            writeFloatWav (referenceFile, rendered.buffer, rendered.sampleRate);

            nlohmann::json manifest;
            manifest["plugin"] = {
                { "name",         rendered.description.name.toStdString() },
                { "manufacturer", rendered.description.manufacturerName.toStdString() },
                { "version",      rendered.description.version.toStdString() },
                { "format",       rendered.description.pluginFormatName.toStdString() },
                { "uid",          rendered.description.createIdentifierString().toStdString() }
            };
            manifest["render"] = {
                { "sample_rate",    rendered.sampleRate },
                { "block_size",     rendered.blockSize },
                { "num_channels",   rendered.buffer.getNumChannels() },
                { "num_samples",    rendered.buffer.getNumSamples() },
                { "length_seconds", rendered.buffer.getNumSamples() / rendered.sampleRate }
            };
            manifest["pluginval_version"] = VERSION;
            manifest["config_hash"] = configHash (config).toStdString();
            manifest["created_on"] = {
                { "os",   juce::SystemStats::getOperatingSystemName().toStdString() },
                { "arch", archString().toStdString() },
                { "date", juce::Time::getCurrentTime().toISO8601 (true).toStdString() }
            };

            config.getReferenceSidecarFile().replaceWithText (manifest.dump (2));

            result.outcome = TestResult::Outcome::referenceCreated;
            return result;
        }

        // Compare mode: warn (don't fail) if the stored config hash differs.
        if (const auto sidecar = config.getReferenceSidecarFile(); sidecar.existsAsFile())
        {
            try
            {
                const auto manifest = nlohmann::json::parse (sidecar.loadFileAsString().toStdString());
                if (manifest.contains ("config_hash")
                    && manifest["config_hash"].get<std::string>() != configHash (config).toStdString())
                {
                    std::cout << "  WARNING: reference was recorded from a different config (stale?): "
                              << sidecar.getFullPathName() << std::endl;
                }
            }
            catch (const std::exception&) { /* a malformed sidecar is non-fatal */ }
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const auto reference = readWav (formats, referenceFile);

        bool allPassed = true;

        const auto comparisonConfig = config.getComparison();

        for (auto it = comparisonConfig.begin(); it != comparisonConfig.end(); ++it)
        {
            const juce::String method (it.key());
            auto comparator = createComparator (method);

            if (comparator == nullptr)
                return TestResult::makeError (name, "unknown comparison method: " + method);

            const auto result2 = comparator->compare (reference, rendered.buffer, it.value());
            allPassed = allPassed && result2.passed;
            result.comparisons.emplace_back (method, result2);
        }

        result.outcome = allPassed ? TestResult::Outcome::passed : TestResult::Outcome::failed;

        if (! allPassed)
        {
            // Write a diff WAV (output - reference) over the overlapping region.
            const int channels = juce::jmin (reference.getNumChannels(), rendered.buffer.getNumChannels());
            const int samples = juce::jmin (reference.getNumSamples(), rendered.buffer.getNumSamples());

            if (channels > 0 && samples > 0)
            {
                juce::AudioBuffer<float> diff (channels, samples);

                for (int c = 0; c < channels; ++c)
                {
                    diff.copyFrom (c, 0, rendered.buffer, c, 0, samples);
                    diff.addFrom (c, 0, reference, c, 0, samples, -1.0f);
                }

                const auto diffFile = config.getDiffFile();
                writeFloatWav (diffFile, diff, rendered.sampleRate);
                result.diffFile = diffFile;
            }
        }

        return result;
    }
    catch (const std::exception& e)
    {
        return TestResult::makeError (name, e.what());
    }
}

//==============================================================================
int runTestFile (const juce::File& configFile)
{
    std::vector<TestConfig> configs;

    try
    {
        configs = TestConfig::loadFromFile (configFile);
    }
    catch (const std::exception& e)
    {
        const auto result = TestResult::makeError (configFile.getFileNameWithoutExtension(), e.what());
        reporter::report (result);
        return reporter::exitCode (result);
    }

    if (configs.empty())
    {
        const auto result = TestResult::makeError (configFile.getFileNameWithoutExtension(), "no test configs in file");
        reporter::report (result);
        return reporter::exitCode (result);
    }

    // v1 runs the first entry only; the parser already accepts arrays for the
    // phase-2 multiplexing extension.
    if (configs.size() > 1)
        std::cout << "Note: " << configs.size() << " configs found; v1 runs the first only." << std::endl;

    const auto result = runTest (configs.front());
    reporter::report (result);
    return reporter::exitCode (result);
}

} // namespace acceptance
