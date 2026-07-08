Import("env")

board_config = env.BoardConfig()
board_config.update("build.hwids", [
    [
        "0x2341", #VID
        "0x0243" #PID
    ]
])
board_config.update("build.usb_product", "3x4 Macro Keyboard")
