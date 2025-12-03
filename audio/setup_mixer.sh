#!/bin/sh

# Wacht 3 seconden om zeker te zijn dat de USB driver geladen is
sleep 3

# De CM6206 nummers kunnen variëren. Check met 'tinymix' via ADB als dit niet werkt.
# Onderstaande namen zijn standaard voor de C-Media driver.

# 1. Master Outputs open zetten
tinymix "Master Playback Volume" 100%
tinymix "Master Playback Switch" 1

# 2. PCM (Android Stream) open zetten
tinymix "PCM Playback Volume" 100%
tinymix "PCM Playback Switch" 1

# 3. Specifieke Kanalen open zetten (Voor 5.1 support)
tinymix "Front Playback Switch" 1
tinymix "Surround Playback Switch" 1
tinymix "Center Playback Switch" 1
tinymix "LFE Playback Switch" 1

# 4. Inputs 'Muten' in de hardware (voorkomt ruis/feedback lus)
#    Android leest de mic digitaal uit, we hoeven hem niet analoog door te lussen.
tinymix "Mic Playback Switch" 0
tinymix "Line Playback Switch" 0