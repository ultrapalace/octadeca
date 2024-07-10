Import("env")

APP_BIN = "$BUILD_DIR/${PROGNAME}.bin"
MERGED_BIN = "$PROJECT_DIR/${PROGNAME}_merged.bin"
BOARD_CONFIG = env.BoardConfig()


def move_bin(source, target, env):
    # The list contains all extra images (bootloader, partitions, eboot) and
    # the final application binary
    # flash_images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])) + ["$ESP32_APP_OFFSET", APP_BIN]

    # Run esptool to merge images into a single binary
    env.Execute(
        " ".join(
            [
                "cp",
                APP_BIN,
                "."
            ]
        )
    )

# Add a post action that runs esptoolpy to merge available flash images
env.AddPostAction(APP_BIN , move_bin)