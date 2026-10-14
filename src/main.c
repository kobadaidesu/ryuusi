/*  main.c  –  エントリポイント
 *
 *  SDL2 は WinMain を期待するので Windows でも
 *   int main(int argc, char* argv[]) を使えばよい
 *  (SDL2 が内部で WinMain を提供してくれる)。
 */

#include "app.h"
#include "bench.h"
#include <stdlib.h>

int main(int argc, char *argv[])
{
    /* --bench などの計測モードの引数 (ブラウザでは Module.arguments) */
    if (bench_configure(argc, argv) != 0)
        return EXIT_FAILURE;

    if (app_init() != 0)
        return EXIT_FAILURE;

    app_run();
    app_shutdown();

    return EXIT_SUCCESS;
}
