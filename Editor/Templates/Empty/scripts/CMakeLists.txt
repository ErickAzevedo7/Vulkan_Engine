# Builds the game's scripts. Every .h and .cpp in this folder goes in, in any folder below it.
cmake_minimum_required(VERSION 3.21)
project(Game CXX)

include("${VKE_SDK_DIR}/VkeGame.cmake")
vke_add_game(Game)
