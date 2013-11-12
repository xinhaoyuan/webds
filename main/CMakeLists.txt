ADD_EXECUTABLE(main main.cpp webds_webkit.cpp webds_desktop.cpp)
ADD_DEPENDENCIES(main webdsutil)
TARGET_LINK_LIBRARIES(main webdsutil ${CMAKE_DL_LIBS} ${GTK3_LIBRARIES} ${WEBKITGTK3_LIBRARIES})
