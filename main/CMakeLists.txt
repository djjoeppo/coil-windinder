idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." "../include"
                    REQUIRES motion_control hx711 gcode_parser stepper)
