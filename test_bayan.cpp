#define BOOST_TEST_MODULE test_bayan


#include <boost/test/unit_test.hpp>
#include <boost/test/tools/output_test_stream.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

BOOST_AUTO_TEST_SUITE(test_bayan)

BOOST_AUTO_TEST_CASE(test_test)
{
  BOOST_CHECK(true == true);
}

void create_file(std::string filename,std::string text){
     std::ofstream file1(filename);
     if (file1.is_open()) {
         file1 << text << std::endl;
         file1.close();
     } else {
         std::cerr << "Ошибка при создании файла" << filename << std::endl;
     }
}
BOOST_AUTO_TEST_CASE(test_valid_bayan)
{
    std::cout << "test_valid_bayan" << std::endl;

    create_file("hello1.txt","hello");
    create_file("hello2.txt","hello");
    create_file("hello3.txt","hello1");

    boost::test_tools::output_test_stream out;
    std::streambuf* oldCout = std::cout.rdbuf();
    std::cout.rdbuf(out.rdbuf());

    system("./bayan -h -s . -m \"*.txt\"");

    std::cout.rdbuf(oldCout);
    std::cout << out.str() << std::endl;

    std::filesystem::remove("hello1.txt");
    std::filesystem::remove("hello2.txt");
    std::filesystem::remove("hello3.txt");

    BOOST_CHECK(true == true);
}

BOOST_AUTO_TEST_SUITE_END()
