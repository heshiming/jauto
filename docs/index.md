---
layout: default
title: Home
description: "JAuto is an open-source lightweight agent for automating Java GUI apps. It lets you keep track of UI components, their coordinates, and states. Talk to JAuto via bash scripts and perform automation using an input simulator."
nav_order: 0
---

# JAuto: Lightweight Java GUI Automation

JAuto is an open-source lightweight agent for automating Java GUI apps. It lets you keep track of UI components, their coordinates, and states. Talk to JAuto via [bash scripts](faq.md#how-do-i-call-jauto-in-a-bash-script) and perform automation using an [input simulator](faq.md#what-input-simulator-can-i-use-with-jauto).

## Benefits

* No target app source code is necessary.
* No Java programming is necessary. Compatible with most programming languages for automation control.
* Shallow learning curve: one way of communication, a handful of commands.
* Very lightweight and efficient, near-zero overhead.
* Works in a headless container.

## Under the hood

JAuto is a <a href="https://docs.oracle.com/javase/8/docs/technotes/guides/jvmti/" target="_blank">JVMTI (Java Virtual Machine Tool Interface)</a> agent, running native code in the Java virtual machine. It is capable of iterating through instances of UI components and fetching their properties. You send a command to JAuto's named pipe to communicate with it. When finished, JAuto writes the output of the command to a file. See [communication protocol](references/communication.md) for details.

The UI component properties JAuto can fetch include:

* Class name and package name in dot notation
* Class name and package name of the base class (beginning with java. or javax.)
* Component screen location and dimensions
* Component-specific attributes such as `text`, `selected`
