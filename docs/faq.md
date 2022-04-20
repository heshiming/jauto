---
layout: default
title: Frequently Asked Questions
description: Frequently Asked Questions for JAuto
nav_order: 3
---

# JAuto Frequently Asked Questions
{: .no_toc }

<details open markdown="block">
  <summary>
    Table of contents
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>

---

## What OS does JAuto support?

JAuto is developed on macOS and tested on Debian Linux. It should work on most Unix-like OSes (Linux, macOS, FreeBSD etc).

---

## Which Java version does JAuto support?

JAuto uses JVMTI version 1.0 and JNI version 1.6. The minimum Java version that meets this requirement is Java 6, or JRE 1.6.

---

## What Java GUI frameworks/toolkit does JAuto support?

JAuto currently supports the [Abstract Window Toolkit (AWT)](https://en.wikipedia.org/wiki/Abstract_Window_Toolkit){:target="_blank"} and the [Swing](https://en.wikipedia.org/wiki/Swing_(Java)){:target="_blank"} framework/toolkit.

I look forward to supporting JavaFX and Standard Widget Toolkit (SWT) soon.

---

## What input simulator can I use with JAuto?

[Xdotool](https://github.com/jordansissel/xdotool){:target="_blank"} is a [generously licensed](https://github.com/jordansissel/xdotool/blob/master/COPYRIGHT){:target="_blank"} [X11](https://en.wikipedia.org/wiki/X_Window_System){:target="_blank"} keyboard and mouse simulation tool. It works flawlessly with Java GUI programs running under X11 in a variety of Linux distros.

Xdotool can use JAuto's `mx`, `my` coordinates directly as mouse click points, followed by keyboard typing.

For instance, these are the steps to fill a `JTextField`:

1.  Call JAuto's [`list_ui_components` command](references/commands/list_ui_components.md) to get `mx`, `my` for the `JTextField`. 
2.  Call `xdotool mousemove $MX $MY click 1 type username`

As you can see from the arguments, `xdotool` is capable of "command chaining", that is, in this case, combining the actions of mouse move, left click, and keyboard typing into a single command.

---

## How do I call JAuto in a bash script?

Below is an example bash function to call a JAuto command:

```bash
function _call_jauto {
    # use a random temporary file
    local OUTPUT_FILE=$(mktemp)
    local CMD=$1
    local OUTPUT=''
    local TIMER=0
    rm -f $OUTPUT_FILE
    # wait until the $JAUTO_INPUT pipe becomes ready
    while [[ ! -p $JAUTO_INPUT ]]; do
        sleep 0.5
    done
    # infinite loop(1) until jauto responds or times out
    while true; do
        # use `timeout` to kill `echo` in case the pipe is broken,
        # preventing an infinite block here
        timeout -k 1s 1s echo "$OUTPUT_FILE|$CMD" > $JAUTO_INPUT
        # get the exit status of `timeout`
        local TIMEOUT_RET=$?
        if [ $TIMEOUT_RET == 0 ]; then
            # `timeout` did not time out, command sent successfully
            local OF_TIMER=0
            # infinite loop(2) until the $OUTPUT_FILE becomes readable
            while [ $OF_TIMER -lt 10 ]; do
                if [ -f $OUTPUT_FILE ]; then
                    # read $OUTPUT_FILE and break loop(2)
                    OUTPUT=$(<"$OUTPUT_FILE")
                    rm -f $OUTPUT_FILE
                    break
                fi
                ((OF_TIMER=OF_TIMER+1))
                sleep 0.5
            done
            # jauto never returns empty, $OUTPUT must be filled at this point,
            # if so break loop(1), otherwise consider it a timeout
            if [ ! -z "$OUTPUT" ]; then
                break
            fi
        fi
        # timeout loop(1) after 10 trials, or about 20 seconds
        # (1 sec `timeout` + 1 sec `sleep` per trial)
        ((TIMER=TIMER+1))
        if [ $TIMER -gt 10 ]; then
            OUTPUT="!timeout!"
            break
        fi
        sleep 1
    done
    echo "$OUTPUT"
}
```

Calling this function is as easy as:

```bash
local OUTPUT=$(_call_jauto "list_ui_components?window_type=dialog")
```

---

## How do I parse JAuto output in a bash script?

Below is an example bash function that can parse **a single line** of [typical JAuto output](references/commands/list_ui_components.md#output) into a bash associative array (a bash 4+ feature).

```bash
function _jauto_parse_props {
    # _jauto_parse_props "A_SINGLE_LINE_OF_COMPONENT"
    # Parse a comma-delimited line such as
    # "javax.swing.JLabel,,x:315,y:549,w:210,h:15,mx:420,my:556,text:Requesting startup parameters.."
    # and return an associative array.
    IFS="," read -ra WIN_PROPS <<< "$@"
    local -A PARSED_PROPS
    FIELD_NUM=0
    for WIN_PROP in "${WIN_PROPS[@]}"; do
        ((FIELD_NUM=FIELD_NUM+1))
        PROP_LEN=${#WIN_PROP}
        PROP_VALUE=${WIN_PROP#*":"}
        if [ $PROP_LEN -eq ${#PROP_VALUE} ]; then
            # PROP_LEN equals PROP_VALUE length means no colon, skip
            PARSED_PROPS["F"$FIELD_NUM]=$PROP_VALUE
            continue
        fi
        PROP_KEY=${WIN_PROP:0:PROP_LEN-${#PROP_VALUE}-1}
        # properly escape $PROP_VALUE
        PROP_VALUE="${PROP_VALUE//\\/\\\\}"
        PROP_VALUE="${PROP_VALUE//\"/\\\"}"
        PARSED_PROPS[$PROP_KEY]=$PROP_VALUE
    done
    echo '('
    for KEY in "${!PARSED_PROPS[@]}"; do
        echo "[$KEY]=\"${PARSED_PROPS[$KEY]}\""
    done
    echo ')'
}
```

You can combine this function with the `_call_jauto` example to run parsing for each line:

```bash
local OUTPUT=$(_call_jauto "list_ui_components?window_type=dialog")
readarray -t COMPONENTS <<< "$OUTPUT"
for COMPONENT in "${COMPONENTS[@]}"; do
    local -A PROPS="$(_jauto_parse_props $COMPONENT)"
    if  [ "${PROPS['F1']}" == "javax.swing.JTree(row)" ] && \
        [ "${PROPS['F2']}" == "Configuration/API/Settings" ]; then
        xdotool mousemove ${PROPS["mx"]} ${PROPS["my"]} click 1
        break
    fi
done
```

---

## Can I embed JAuto in my product?

JAuto is available under the [GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html){:target="_blank"} license as well as a commercial license. Users choosing to use JAuto under the free, open-source license must comply with its terms. Alternatively, users may choose to purchase a commercial license, which enables the distribution of JAuto in any form without restrictions.
