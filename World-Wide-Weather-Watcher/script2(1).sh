#!/bin/bash

# Set base directory to the location of this script (useful for running from GitHub directory)
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_PATH="/home/ramimohamed/Arduino/xd3.0/"  # Path to the sketch relative to the script
BOARD_FQBN="arduino:avr:uno"         # FQBN for Arduino Uno
OUTPUT_PATH="$BASE_DIR/build"        # Output directory for compiled files in the script's directory
SERIAL_PORT="/dev/ttyACM0"           # Serial port for Arduino
BAUD_RATE=9600                       # Baud rate for serial monitor


# Create the output directory if it doesn't exist
mkdir -p "$OUTPUT_PATH"

# Compile the sketch
echo "Compiling the sketch..."
arduino-cli compile --fqbn "$BOARD_FQBN" "$SKETCH_PATH" --output-dir "$OUTPUT_PATH"

# Check if the compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful! Compiled files are in $OUTPUT_PATH"
else
    echo "Compilation failed."
    exit 1
fi

# Upload the compiled code to the Arduino board
echo "Uploading the sketch..."
arduino-cli upload -p "$SERIAL_PORT" --fqbn "$BOARD_FQBN" "$SKETCH_PATH"

# Check if the upload was successful
if [ $? -ne 0 ]; then
    echo "Upload failed."
    exit 1
fi

# Set the baud rate and open the serial monitor
echo "Opening the serial monitor..."
stty -F "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb
cat "$SERIAL_PORT"

