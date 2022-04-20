---
layout: default
title: ping
description: JAuto ping command reference
parent: Commands
grand_parent: References
nav_order: 0
---

# ping

The `ping` command can verify whether JAuto is working. This command has no arguments.

When used with an output file, i.e. `$ echo "/tmp/test.txt|ping" > /tmp/jauto.in`, JAuto will write `pong` into the file `/tmp/test.txt`. Getting the `pong` means JAuto is ready for command processing.

When used without an output file, i.e. `$ echo "|ping" > /tmp/jauto.in`, it can verify whether the input named pipe of JAuto is working. During JAuto startup, the named pipe appears in a short time before JAuto becomes ready. If `$ echo "|ping" > /tmp/jauto.in` returns immediately, it means either JAuto has started or is starting. If `echo` blocks, it means either JAuto did not start or it is malfunctioning.

