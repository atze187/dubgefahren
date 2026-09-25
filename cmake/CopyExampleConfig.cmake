# Legt die Beispiel-Config ins Build-Bundle, falls dort noch keine liegt.
# Aufruf: cmake -DBUNDLE=<pfad/Dubgefahren.vst3> -DSOURCE=<Beispiel-Config> -P CopyExampleConfig.cmake
set(dest "${BUNDLE}/Contents/Resources/Dubgefahren.config.json")
if(NOT EXISTS "${dest}")
    file(MAKE_DIRECTORY "${BUNDLE}/Contents/Resources")
    file(COPY_FILE "${SOURCE}" "${dest}")
endif()
