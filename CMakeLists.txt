idf_component_register(SRCS "COTASystem.cpp" "CDateTimeSystem.cpp" "CNvsSystem.cpp" "CSpiffsSystem.cpp" 
                    "CJsonParser.cpp"
                    "CBufferSystem.cpp"
                    "CJsonReadStream.cpp"
                    "CDateTimeSystem.cpp"
                    "CCRC8.cpp"
                    "CCRC16.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES jsmn spiffs nvs_flash app_update)
