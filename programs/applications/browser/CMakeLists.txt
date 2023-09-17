SET(SOURCES main.cpp HTMLParser.cpp Node.cpp)
MAKE_APP(browser)
TARGET_LINK_LIBRARIES(browser libui)