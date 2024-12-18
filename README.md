# CMP501_CorporateLife
 CMP501 - Networking Final Assessment (Abertay University)


Name: Harshita Shetye
Student ID: 2406593

Github Repository Link: https://github.com/harshita-shetye/CMP501_CorporateLife

Video Link: https://youtu.be/KVgXBkgQiGg

How to run the program:

1. Launch the server.exe
2. Launch the client.exe. Enter your name, and the IPv4 address to connect to the server.
3. You will be taken to a waiting lobby menu until a second player connects. Once the second player connects, the game will launch.
4. Gameplay: Collide with the rainbow dot to gain 1 point. Grey shape is your actual local position, which is sent to the server. 
Green circle shape is your predicted position which is received from the server. Red shape is the opponent's circle shape.


SFML Version: SFML-2.6.1

Link External Libraries:
1. Debug: Properties > Linker > Input > sfml-network-d.lib;sfml-graphics-d.lib;sfml-audio-d.lib;sfml-main-d.lib;sfml-system-d.lib;sfml-window-d.lib;$(CoreLibraryDependencies);%(AdditionalDependencies)
2. Release: Properties > Linker > Input > sfml-network.lib;sfml-graphics.lib;sfml-audio.lib;sfml-main.lib;sfml-system.lib;sfml-window.lib;$(CoreLibraryDependencies);%(AdditionalDependencies)