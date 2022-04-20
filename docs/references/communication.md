---
layout: default
title: Communication Protocol
description: "JAuto communication protocol: explanation on mechanisms and syntax."
parent: References
nav_order: 0
---

# Communication Protocol

Once started, you can communicate with JAuto using named pipe and file system. <a href="https://en.wikipedia.org/wiki/Named_pipe" target="_blank">Named pipe</a> is a common mechanism for <a href="https://en.wikipedia.org/wiki/Inter-process_communication" target="_blank">inter-process communication</a>. It is well-supported by all mainstream operating systems and most programming languages. Reading from and writing to a pipe is a lot like a file. You can even send a command using <a href="https://www.gnu.org/software/bash/" target="_blank">bash</a>:

    $ echo "/tmp/test.txt|ping" > /tmp/jauto.in

In the above command, `/tmp/jauto.in` is the path to the "input" named pipe created by JAuto. It is used exclusively as a read-only pipe, that is, JAuto will not send anything back to this pipe. You can configure the path by specifying a [start-up argument]().

When command processing is finished, JAuto will write a file containing the output or the results. In the above case, the output will be written into `/tmp/test.txt`.

## Command Syntax

JAuto command syntax has 4 parts (denoted as A, B, C, and D below), followed by a return.

    /path/to/outputfile|command?arg1=value&arg2=value
    [--------A--------]B[--C--][---------D----------]

A return indicates the end of a command. Therefore, all arguments must fit in one line.

### Part A (Optional)

Part A specifies the output file name to which JAuto will write the output. The target Java program must have sufficient privilege to write to this location. If this part is omitted, in which case the commands start with the `|` character, the output will be discarded.

### Part B

Part B is the <a href="https://en.wikipedia.org/wiki/Vertical_bar" target="_blank">vertical bar</a> or the "pipe" character `|`.

### Part C

Part C is the command. Examples include: `ping`, `get_windows`, `list_ui_components`.

### Part D (Optional)

Part D is the arguments for the command. This part is optional. Its syntax looks and works like a <a href="https://en.wikipedia.org/wiki/Query_string" target="_blank">query string</a>. There is a question mark `?` right after the command indicating that additional arguments are following. Multiple arguments are separated by the ampersand `&`. And for each argument, name, and value are separated by an equal sign `=`.

Note that JAuto only tokenizes these characters but does not attempt actual URL decoding. Hence, you should not escape the arguments (do not replace space with `+` or `%20`). JAuto does not support special characters `?` `&` `=` in argument names or values.

## Output File Format

An output file contains text-based, comma-delimited records. Each record uses one line. If there are return characters in the line, it will be escaped into `\n`. Below is an example output file:

    javax.swing.JDialog,feature.configure.ah,window:0,x:182,y:129,w:768,h:576,act:1,title:DU278752 Trader Workstation Configuration (Simulated Trading)
    javax.swing.JPanel,feature.configure.ak,x:185,y:163,w:762,h:545,mx:566,my:435
    javax.swing.JButton,jtscomponents.hf,x:458,y:667,w:65,h:23,mx:490,my:678,selected:n,text:OK
    javax.swing.JButton,jtscomponents.hf,x:533,y:667,w:65,h:23,mx:565,my:678,selected:n,text:Apply
    javax.swing.JButton,jtscomponents.hf,x:608,y:667,w:65,h:23,mx:640,my:678,selected:n,text:Cancel
    javax.swing.JSplitPane,jtscomponents.j4,x:185,y:163,w:762,h:486,mx:566,my:406
    javax.swing.JPanel,,x:185,y:163,w:295,h:486,mx:332,my:406
    javax.swing.JPanel,feature.configure.panels.generic.ab,x:185,y:163,w:295,h:486,mx:332,my:406
    javax.swing.JScrollPane,,x:185,y:163,w:295,h:486,mx:332,my:406
    javax.swing.JViewport,,x:185,y:163,w:295,h:486,mx:332,my:406
    javax.swing.JTree,feature.configure.panels.generic.ConfigurationTree,x:185,y:163,w:295,h:486,mx:332,my:406
    javax.swing.JTree(row),Configuration,x:185,y:163,w:111,h:20,mx:240,my:173
    javax.swing.JTree(row),Configuration/General,x:205,y:183,w:72,h:16,mx:241,my:191
    javax.swing.JTree(row),Configuration/Lock and Exit,x:205,y:199,w:105,h:16,mx:257,my:207
    javax.swing.JTree(row),Configuration/API,x:205,y:231,w:42,h:16,mx:226,my:239
    javax.swing.JTree(row),Configuration/API/Settings,x:225,y:247,w:75,h:16,mx:262,my:255
    javax.swing.JPanel,feature.configure.panels.generic.ac,x:499,y:251,w:616,h:493,mx:807,my:497
    javax.swing.JPanel,feature.configure.panels.custom.driver_panels.ag,x:499,y:251,w:616,h:16,mx:807,my:259
    javax.swing.JCheckBox,trader.common.tag.m,x:499,y:251,w:105,h:16,mx:551,my:259,selected:n,text:Read-Only API

The first two comma-delimited values (`javax.swing.JDialog,feature.configure.ah` in the first line above) are **unnamed attributes**. Attribute #1 is the "Java base class name", that is, the `javax.` or `java.` class that the instance class inherited from. Attribute #2 is the "class name", the name the target program uses, or the name of the custom inherited class. If the component is using the Java base class directly, then attribute #2 is empty.

In the example, `javax.swing.JDialog,feature.configure.ah` means the "Java base class name" is `javax.swing.JDialog`, and the target program inherited `JDialog` and created a custom new class named `feature.configure.ah`.

The rest attributes are **named attributes**. They follow a `field_name:field_value` format. For more information on what each `field_name` means, please refer to [the individual command reference](commands.md).

JAuto never writes an empty output file. If for any reason, no windows or no components could be listed, JAuto writes `none` in the output file.
