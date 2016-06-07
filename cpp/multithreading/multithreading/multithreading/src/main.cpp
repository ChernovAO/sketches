/// @file
/// @brief Таки парсинг

#include <string>
#include <iostream>
#include <regex>
#include <list>

const char* PolynomialSubpart = "[+-]*[0-9]*\\**x\\^*[0-9]*";

const char* DegreePatern = "x\\^*[0-9]*";

typedef std::list< std::string > PolynomialSubparts;

void DivideAPolynomial( const std::string& source, PolynomialSubparts& subparts )
{
     std::regex regExp( PolynomialSubpart );

     auto firstSubPolinomial = std::sregex_iterator( source.begin()
                                                     , source.end()
                                                     , regExp );

     for ( auto i = firstSubPolinomial; i != std::sregex_iterator(); ++i )
     {
          subparts.push_back( i->str() );
     }
}

int GetDegree( const std::string& str )
{
     std::regex degreeRegEx( DegreePatern );

     auto degreeIterator = std::sregex_iterator( str.begin(), str.end(), degreeRegEx );

     int resultDegree = 1;
     if ( degreeIterator != std::sregex_iterator() )
     {
          resultDegree = std::stoi( degreeIterator->str() );
     }

     if ( ++degreeIterator != std::sregex_iterator() )
     {
          throw std::logic_error( "Invalid degree matching" );
     }

     return resultDegree;
}

int main(int argc, char* argv[])
{
     std::string originalPolynomial;
     std::cin >> originalPolynomial;

     PolynomialSubparts subparts;
     DivideAPolynomial( originalPolynomial, subparts );

     return 0;
}
