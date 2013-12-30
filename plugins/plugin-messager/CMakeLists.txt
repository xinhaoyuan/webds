ADD_LIBRARY(webdsplugin-messager SHARED main.cpp)
ADD_DEPENDENCIES(webdsplugin-messager webdsutil)
TARGET_LINK_LIBRARIES(webdsplugin-messager webdsutil ${CMAKE_DL_LIBS} ${GTK3_LIBRARIES} ${WEBKITGTK3_LIBRARIES})
