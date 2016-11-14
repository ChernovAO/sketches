/// @brief Code sketch to test boost::program_options behavior.

#include <boost/program_options.hpp>

#include <string>
#include <iomanip>
#include <iostream>
#include <exception>

using namespace boost::program_options;

const char* help_full = "help,h";
const char* help_short = "help";
const char* help_desc = "Help";

const char* first_command = "first-command";
const char* first_desc = "Some first command";

const char* second_command = "secodn-command";
const char* second_desc = "Some second command";

const char* first_options_a = "a-options,a";
const char* first_options_a_desc = "Some options called A";

const char* first_options_b = "b-options,b";
const char* first_options_b_desc = "Some options called B";

const char* second_options_c = "c-options,c";
const char* second_options_c_desc = "Some options called C";

const char* second_options_d = "d-options,d";
const char* second_options_d_desc = "Some options called D";


int main( int argc, char** argv )
{
    try
    {
        options_description general( "General" );
        general.add_options()
            ( help_full, help_desc )
            ( first_command, first_desc )
            ( second_command, second_desc );

        auto parsed = command_line_parser( argc, argv )
            .options( general )
            .allow_unregistered()
            .run();

        variables_map vm;
        store( parsed, vm );

        if ( vm.empty() || vm.count( help_short ) != 0 )
        {
            std::cout << general << std::endl;
            return 0;
        }

        options_description first_options( "First possible options" );
        first_options.add_options()
            ( first_options_a, value< std::string >()->required(), first_options_a_desc )
            ( first_options_b, value< std::string >()->required(), first_options_b_desc );

        auto command_options = collect_unrecognized( parsed.options, include_positional );

        if ( vm.count( first_command ) )
        {
            std::string a;
            options_description first_options( "First possible options" );
            first_options.add_options()
                ( first_options_a, value< std::string >( &a )->required(), first_options_a_desc )
                ( first_options_b, value< std::string >()->required(), first_options_b_desc );

            auto command_options = collect_unrecognized( parsed.options, include_positional );
            auto parsed_options = command_line_parser( command_options )
               .options( first_options )
               .run();

            variables_map options_vm;
            store( parsed_options, options_vm );
            notify( options_vm );
        }
    }
    catch( const std::exception& e )
    {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}
