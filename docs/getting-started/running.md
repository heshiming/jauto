---
layout: default
title: Running JAuto
description: Running JAuto guide.
parent: Getting Started
nav_order: 1
---
# Running JAuto

JAuto is a <a href="https://docs.oracle.com/javase/8/docs/technotes/guides/jvmti/" target="_blank">JVMTI (Java Virtual Machine Tool Interface)</a> agent. It runs in the JVM with a target Java program. To run JAuto, first figure out the command line to run the target Java program, which is:

    $ java -jar TargetProgram.jar

Then add an argument to specific loading of JAuto, like the following:

    $ java -jar TargetProgram.jar -agentpath:/path/to/jauto.so

Some programs may come with a launch script. You will need to inspect the launch script source to understand whether it provided a mechanism to add JVM arguments, or you have to modify the script directly to make it work. A common mechanism is a <a href="https://www.ej-technologies.com/resources/install4j/help/doc/concepts/vmParameters.html">`.vmoptions` file</a>, which is a text file where you can add JVM arguments one per line.

For more information regarding the `agentpath` argument, please refer to <a href="https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#starting">JVMTI documentation</a>.

A successful start of the target Java program typically means a successful start of JAuto. To verify it, enter the following command in a shell:

    $ echo "/tmp/test.txt|ping" > /tmp/jauto.in

If JAuto is working, it will write `pong` into file `/tmp/test.txt`. Inspect its content by:

    $ cat /tmp/test.txt

Due to the threaded and asynchronous processing of commands in JAuto, there might be a small delay in file generation. It is normal if the file did not immediately appear after entering the command.
