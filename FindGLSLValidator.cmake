# Find the glsl validator executable
find_program(GLSL_VALIDATOR_EXECUTABLE
    NAMES glslangValidator
    DOC "GLSL Language Validator"
)
mark_as_advanced(GLSL_VALIDATOR_EXECUTABLE)