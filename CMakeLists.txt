idf_component_register(SRCS "COTASystem.cpp" "CDateTimeSystem.cpp" "CNvsSystem.cpp" "CSpiffsSystem.cpp" 
                    "CJsonParser.cpp"
                    "CBufferSystem.cpp"
                    "CJsonReadStream.cpp"
                    "CDateTimeSystem.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES jsmn spiffs nvs_flash app_update)
