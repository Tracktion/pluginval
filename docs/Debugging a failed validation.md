# Debugging a Failed Validation

If your plugin fails validation with `pluginval` you'll want to debug it properly to find out why.

You may be running tests locally using `pluginval` or you might only have access to the log files. These can come from several different sources including your own CI, other developers or even end users. It's important to try to replicate test environments quickly in order to get to the root cause of the problem.

### Quick Debugging
It's a good idea to do a simple quick debug by attempting to validate the problem plugin yourself. Often the same issue is repeatable without the advanced settings listed below.

The main thing here is to build `pluginval` from source, run it in the debugger and then perform a validation. The three important steps you'll need to follow are:

1. Build `pluginval` in debug and run it from the IDE
2. Set **validate in process** to **on**
3. Ensure the **strictness level** is the same as the reported log

Although it's not strictly necessary to validate in process, doing so means the debugger will stop on any crashes and you can add breakpoints to catch test failures. For this you can do one of two things:
1. Add a breakpoint in `UnitTestRunner::addFail`
2. Search the `pluginval` source for `testRunner.setAssertOnFailure (false);` and change this to true

Once you've done these, run the test and hopefully you'll see exactly where and why the test failed and you can work on a fix.


### Advanced Debugging
Sometimes the quick steps above won't unearth the problem. This could be that the test configuration isn't exactly the same as the failed validation. Whilst it's impossible to replicate everything about a plugin from `pluginval` only (for example plugin-internal random states), it is possible to replicate the tests and values `pluginval` uses.
From the failed log file, look for the following settings:
 - Random seed
 - Timeout period
 - Number of test repeats
 - Randomise test order


 Use either the UI or command line options to ensure these are set to the same values as the failed validation. This should mean any parameter values, order of tests, MIDI note values, durations etc. are identical to the failed test and thus maximise the chance of replicating issues.

### Debugging a command line validation from an IDE
If the failed report came from a CLI invocation of `pluginval`, you might want to debug it in the same way.
The simplest way to do this is provide your command line arguments as you run `pluginval` via the IDE. This will vary between IDE but is usually listed as something like "Arguments passed on launch" or "Command line". If you provide a valid path to a plugin here, `pluginval` will start in command line mode rather than show the UI and jump straight in to a validation.

One final thing you might need to check is that any environment variables that were set in the failed validation are also set in the IDE. Again, these vary between IDE but are usually listed as "Environment variables" under the "Run" options.
