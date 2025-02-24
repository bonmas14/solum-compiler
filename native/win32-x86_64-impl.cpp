#include "backend.h"
#include <stdlib.h>

struct codegen_state_t {
    bool deez;
};

void generate_code(compiler_t *compiler) {
    log_update_color();
    fprintf(stdout, "WE ARE IN CODEGEN\n");

    FILE * file = fopen("./temp/output.cpp", "wb");

    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include \"type_defines.h\"\n");

    fprintf(file, "int main(void) {\n");
    // we do our code here for now
    


    fprintf(file, "    return 0;\n");
    fprintf(file, "}\n");

    fclose(file);
}
