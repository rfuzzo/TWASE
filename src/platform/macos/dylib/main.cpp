#include "stdafx.hpp"

#include "App.hpp"
#include "Image.hpp"
#include "Utils.hpp"

// libTWASE.dylib is injected with DYLD_INSERT_LIBRARIES, these run before / after the game's main()

__attribute__((constructor)) static void TWASE_Load()
{
    try
    {
        const auto image = Image::Get();
        if (!image->IsAttila())
        {
            return;
        }

        App::Construct();
    }
    catch (const std::exception& e)
    {
        SHOW_MESSAGE_BOX_AND_EXIT_FILE_LINE("An exception occured while loading TWASE.\n\n{}", e.what());
    }
    catch (...)
    {
        SHOW_MESSAGE_BOX_AND_EXIT_FILE_LINE("An unknown exception occured while loading TWASE.");
    }
}

__attribute__((destructor)) static void TWASE_Unload()
{
    try
    {
        if (!App::Get())
        {
            return;
        }

        App::Destruct();
    }
    catch (...)
    {
        // the process is exiting, nothing sensible left to do
    }
}
