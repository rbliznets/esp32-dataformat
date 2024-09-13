idf_component_register(SRCS "CDateTimeSystem.cpp" "CNvsSystem.cpp" "CSpiffsSystem.cpp" 
                    "CJsonParser.cpp"
                    "CBufferSystem.cpp"
                    "CJsonReadStream.cpp"
                    "CDateTimeSystem.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES jsmn spiffs nvs_flash)
