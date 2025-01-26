#!/bin/bash
#Install curl library

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if [ -f /etc/debian_version ]; then
        echo "Detected Debian-based distribution. Installing libcurl using apt..."
        sudo apt update && sudo apt install -y libcurl4-openssl-dev
    elif [ -f /etc/redhat-release ]; then
        echo "Detected Red Hat-based distribution. Installing libcurl using yum..."
        sudo yum install -y libcurl-devel
    elif [ -f /etc/arch-release ]; then
        echo "Detected Arch-based distribution. Installing libcurl using pacman..."
        sudo pacman -Syu curl
    else
        echo "Unknown Linux distribution."
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS. Installing libcurl using Homebrew..."
    brew install curl
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    echo "Please download and install libcurl from https://curl.se/windows/"
else
    echo "Unsupported operating system."
fi
