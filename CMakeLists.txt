idf_component_register(SRCS "CSpiffsSystem.cpp" 
                    "CJsonParser.cpp"
                    "CBufferSystem.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES jsmn spiffs)
