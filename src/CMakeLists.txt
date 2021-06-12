add_executable(pbf-inspector inspector.cpp)
target_link_libraries(pbf-inspector linenoise)

# # Make sure you link your targets with this command. It can also link libraries and
# # even flags, so linking a target that does not exist will not give a configure-time error.
# target_link_libraries(MyExample PRIVATE MyLibExample)