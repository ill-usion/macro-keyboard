# Macro Keyboard

## How to configure using serial

### 1. Read
Reads and returns the saved macro configuration:

```json
{
    "event": "r"
}
```

### 2. Write
Configures a macro 
For key macros:
```json
{
    "event": "w",
    
    // depends on the number of macros, but in our case it's 3 so from 0-2
    // here we're setting the first button
    "macroIndex": 0,

    // 0: key macro
    // 1: text macro     
    "macroType": 0, 

    // an array of SequenceAction (see KeyMacro.h)
    "macroData": [
        {
            "sequenceType": 1, // keystroke
            "keycode": 244 // left ctrl
        },
        {
            "sequenceType": 1, // keystroke
            "keycode": 6 // c
        },
        {
            "sequenceType": 3, // delay
            "delay": 100 // 100ms
        },
        {
            "sequenceType": 0 // release all
        },
        {
            "sequenceType": 2, // consumer keystroke (media controls and such)
            "keycode": 233 // volume up
        },
    ]
}
```

For text macros:
```json
{
    "event": "w",

    // set the 3rd button
    "macroIndex": 2, 

    // 1 indicates a text macro
    "macroType":1,

    // text to be printed
    "macroData": "Hello, World! 1234567890 abcdefghijklmnopqrstuvwxyz"
}

```

### 3. Clear
Clears and resets the EEPROM where the configuration is stored
```json
{
    "event": "x"
}
```