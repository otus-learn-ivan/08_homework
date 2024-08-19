#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <dirent.h>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/crc.hpp>
#include <filesystem>
#include <algorithm>

#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

#include <boost/program_options.hpp>

#include <boost/version.hpp>

#include <boost/filesystem.hpp>

namespace po = boost::program_options;


using namespace std;

struct Ikey{
    virtual void process_bytes (void const *   buffer,std::size_t  byte_count)=0;
    virtual bool operator !=( unique_ptr<Ikey>& other) = 0;
    virtual bool operator <( unique_ptr<Ikey>& other)=0;
    virtual std::ostream& print_key(std::ostream& ss)  const =0;
};
std::ostream& operator << (std::ostream&  ss,  const unique_ptr<Ikey>& key){
    return key->print_key(ss);
}
class Tkey_boost_md5 : public Ikey{
public:
    std::vector<std::array<int,4>> hash;
    void process_bytes (void const *   buffer,std::size_t  byte_count)override{
        boost::uuids::detail::md5 key_;
        key_.process_bytes(buffer,byte_count);
        boost::uuids::detail::md5::digest_type hash_;
        key_.get_digest(hash_);
        std::array<int,4> buf{};
        std::copy(begin(hash_),end(hash_),buf.begin());
        hash.push_back(buf);
    }
    bool operator !=( unique_ptr<Ikey>& other)override{
        auto& other_ = (dynamic_cast< Tkey_boost_md5&>(*other));
        if(hash.size()!=other_.hash.size())return true;
        return !std::equal(hash.begin(),hash.end(),other_.hash.begin());
    }
    bool operator <( unique_ptr<Ikey>& other)override{
        auto& other_ = (dynamic_cast< Tkey_boost_md5&>(*other));
        return std::lexicographical_compare(hash.begin(),hash.end(),other_.hash.begin(),other_.hash.end());
    }
    std::ostream&  print_key(std::ostream&  ss) const override{
        for (auto i: hash.back()) {
            ss <<  boost::format("%02x") % i << " ";
        }
        return ss;
    }
};

class Tkey_boost_crc32 : public Ikey{
public:
    boost::crc_32_type crc32;
    void process_bytes (void const *   buffer,std::size_t  byte_count)override{
        crc32.process_bytes(buffer,byte_count);
    }
    bool operator !=( unique_ptr<Ikey>& other)override{
        return crc32.checksum()!= dynamic_cast< Tkey_boost_crc32&>(*other).crc32.checksum();
    }
    bool operator <( unique_ptr<Ikey>& other)override{
        return crc32.checksum()< dynamic_cast< Tkey_boost_crc32&>(*other).crc32.checksum();
    }
    std::ostream&  print_key(std::ostream&  ss) const override{
        ss << crc32.checksum();
        return ss;
    }
};


bool operator !=( unique_ptr<Ikey>& first,unique_ptr<Ikey>& other){
    if(first == nullptr || other==nullptr) return true;
    return first->operator !=(other);
}

bool operator <( unique_ptr<Ikey>& first,unique_ptr<Ikey>& other){
    return first->operator <(other);
}

struct Ikey_Factory{
    virtual ~Ikey_Factory() = default;
    virtual unique_ptr<Ikey> create_Ikey() const = 0;
};

template<class T>
struct Concrete_Ikey_Factory: public Ikey_Factory{
    unique_ptr<Ikey> create_Ikey() const override{
        return make_unique<T>();
    }
};

using Tkey_boost_crc32_factory = Concrete_Ikey_Factory<Tkey_boost_crc32>;
using Tkey_boost_md5_factory = Concrete_Ikey_Factory<Tkey_boost_md5>;


struct Tfile_reg{
    std::string name;
    std::unique_ptr<boost::interprocess::mapped_region> region;
    unique_ptr<Ikey> crc_key;
    char* begin;
    Tfile_reg( std::string name,std::unique_ptr<boost::interprocess::mapped_region> region_,Ikey_Factory* factory_ikey):
        name(name),
        region(std::move(region_)),
        begin ( static_cast<char*>(region->get_address())){
        crc_key = factory_ikey->create_Ikey();
    }
    char* end(){
        return static_cast<char*>(region->get_address())+region->get_size();
    }
};

#include <boost/regex.hpp>
boost::regex maskToRegex(const std::string& mask) {
    const boost::regex esc("[\\.]");
    const std::string rep("\\\\.");
    std::string result = regex_replace(mask, esc, rep);
    const boost::regex  esc1 ("[\\*]");
    const std::string rep1("\\.*");
    std::string result1 = regex_replace(result, esc1, rep1);
    const boost::regex  esc2 ("[\\?]");
    const std::string rep2("\\.");
    std::string result2 = "^"+regex_replace(result1, esc2, rep2)+"$";
    return boost::regex(result2);
}


struct Tinclude_prm{
    int scan_level;
    unsigned long int min_file_size;
    vector<boost::regex> mask_name_files;
};

struct Tcompare_prm{
    int size_blok;
};

enum Tdept_limit{NOT_LIMIT=-2,END_LIMIT=-1};

struct T_full_directory{
    Tinclude_prm &include_prm;
    int dept;
    std::vector<Tfile_reg> &files;
    const vector<std::string>& do_not_check_dirs;
    T_full_directory(Tinclude_prm& prm,std::vector<Tfile_reg> &files,const vector<std::string>& not_check):include_prm(prm), dept(prm.scan_level),files(files),do_not_check_dirs(not_check){}
    static T_full_directory factory(Tinclude_prm& prm,std::vector<Tfile_reg> &files,const vector<std::string>& not_check){
        return T_full_directory(prm,files,not_check);
    }
    T_full_directory(T_full_directory& other):include_prm(other.include_prm),dept(other.dept-1),files(other.files),do_not_check_dirs(other.do_not_check_dirs){}
    static T_full_directory factory(T_full_directory& other){
        return T_full_directory(other);
    }
    void operator()(const std::string& path_entry, Ikey_Factory* factory_ikey ){
        if(dept==END_LIMIT)return;
        if(std::any_of(do_not_check_dirs.begin(),do_not_check_dirs.end(),[&path_entry](std::string cur_dir_name){return cur_dir_name == path_entry;})){
            return; //исключаем данную дерикторию случай если корневая директория в списке исключённых
        }
        std::filesystem::directory_iterator dir(path_entry);
        if (dir != std::filesystem::directory_iterator{}) {
            // std::filesystem::directory_iterator end;
            for (const std::filesystem::directory_entry & begin : dir ) {
                if(begin.is_regular_file()){
                try{
                        if(begin.file_size()<include_prm.min_file_size){ //в сравнении учавствуют только файлы с размером более min_file_size
                            continue;
                        }
                        if(std::any_of(include_prm.mask_name_files.begin(),include_prm.mask_name_files.end(),[&begin](auto& mask){
                            return !regex_match(begin.path().filename().string(),mask);
                        })){
                            continue;
                        }
                        const boost::interprocess::mode_t mode = boost::interprocess::read_only;
                        boost::interprocess::file_mapping fm(begin.path().string().c_str(), mode);
                        std::unique_ptr<boost::interprocess::mapped_region> region = make_unique<boost::interprocess::mapped_region>(fm, mode, 0, 0);
                        files.push_back(Tfile_reg{begin.path().string(),std::move(region),factory_ikey}); // Добавляем имя файла в вектор
                    }
                    catch(const std::exception& e){
                        std::cerr << e.what() <<" "<<begin.path().string() <<'\n';
                    }
                }
                if(begin.is_directory()){
                   if(std::any_of(do_not_check_dirs.begin(),do_not_check_dirs.end(),[&begin](std::string cur_dir_name){return cur_dir_name == begin.path().string();})){
                       break; //исключаем данную дерикторию
                   }
                   T_full_directory::factory(*this)(begin.path().string(),factory_ikey);
                }
            }
        } else {
            std::cerr << "Ошибка открытия директории " << path_entry << "\n";
        }
    }
};

class Tfile_deletion_condition{
public:
     std::vector<Tfile_reg>& files;
     Tfile_deletion_condition  (std::vector<Tfile_reg>& files_):files(files_){}
     bool operator()(Tfile_reg& file){
        return std::all_of(files.begin(), files.end(), [&file](auto& other){return other.crc_key != file.crc_key || &other == &file;});
     }
};

class Tdirectori{
    std::string path;
    static std::vector<Tfile_reg> files;
    static vector<std::string> do_not_check_dirs;
public:
    static Ikey_Factory* factory_ikey;
    static void do_not_check_dirs_push_bac(std::string&not_chekc){
        do_not_check_dirs.push_back(not_chekc);
    }
    Tdirectori(std::string& path_,Tinclude_prm& dept):path(path_){
        open_files_of_dir(dept);
    }
    void open_files_of_dir(Tinclude_prm& dept){
        T_full_directory::factory(dept,files,do_not_check_dirs)(path,factory_ikey); //!!! добавление файлов директории в процесс расчета если удовлетворяют условиям
    }
    static void compare_files_dirs(Tcompare_prm& compare_prm){
        bool end_proces = true;
        while(end_proces){
            end_proces = false;
            for(auto& file: files){
                if(file.begin < file.end()){
                    auto size_blok = (file.begin +compare_prm.size_blok)<file.end()?compare_prm.size_blok:file.end()-file.begin;
                    file.crc_key->process_bytes(file.begin,size_blok);
                    file.begin +=size_blok;
                    if(file.begin < file.end()) end_proces = true;
                }
            }
            sort(files.begin(),files.end(),[](auto& a,auto& b){
                return a.crc_key < b.crc_key;
            });
            boost::remove_erase_if(files, Tfile_deletion_condition(files));
        }
        cout <<"-- answer: " << files.size() <<" --------------------------------\n";
        for(auto it = files.begin();it!=files.end();it++){
            cout << "\t" << it->name << "crc: " << it->crc_key <<"\n";
            if(it+1 !=files.end()){
                if(it->crc_key !=(it+1)->crc_key){ cout << "\n";}
            }
        }
    }
    std::string get_path(){
        return path;
    }
};

vector<std::string> Tdirectori::do_not_check_dirs;
std::vector<Tfile_reg> Tdirectori::files;
Tkey_boost_crc32_factory boost_crc32_factory;
Tkey_boost_md5_factory boost_md5_factory;
Ikey_Factory* Tdirectori::factory_ikey = &boost_crc32_factory;

std::ostream& operator<< (std::ostream& cout_,Tdirectori& dir_){
    return cout_<< dir_.get_path();
}

struct Tinint_prm{
    enum  {DEFAULT_SIZE_BLOK = 100};
    enum  {DEFAULT_MIN_FILE_SZ = 1};
    vector<string> scan_directories;
    vector<string> except_directories;
    Tinclude_prm include_prm{NOT_LIMIT,DEFAULT_MIN_FILE_SZ,{}};
    Tcompare_prm compare_prm {DEFAULT_SIZE_BLOK};
    string hesh="crc32";
    Tcompare_prm& creator_Tdirectori(){
        std::unique_ptr<std::vector<std::unique_ptr<Tdirectori>>> directories = make_unique<std::vector<std::unique_ptr<Tdirectori>>>() ;
        for(auto& dir: except_directories){
            Tdirectori::do_not_check_dirs_push_bac(dir);
        }
        if(hesh =="md5"){
            Tdirectori::factory_ikey = &boost_md5_factory;
        }
        for(auto& dir: scan_directories){
            directories->push_back(make_unique<Tdirectori>(dir,include_prm));
        }
        return compare_prm;
    }
};


Tinint_prm start_without_argc(){
    Tinint_prm inint_prm;
    std::string dir_one;
    std::cout << "Введите список дерикторий для проверки(пустая строка заканчивает список):" << std::endl;
    while (getline(cin, dir_one)) {
        if(dir_one=="")break;
        inint_prm.scan_directories.push_back(dir_one);
    }
    std::cout << "Введите список дерикторий исключаемых из проверки (пустая строка заканчивает список):" << std::endl;
    while (getline(cin, dir_one)) {
        if(dir_one=="")break;
        inint_prm.except_directories.push_back(dir_one);
    }
    std::cout << "Уровень сканирования (один на все директории, 0 - только указанная директория без вложенных):" << std::endl;
    getline(cin, dir_one);if(!dir_one.empty()){
        inint_prm.include_prm.scan_level = stoi(dir_one);
    }
    dir_one="";
    std::cout << "Минимальный размер файла, по умолчанию проверяются все файлы больше 1 байта." << std::endl;
    getline(cin, dir_one);if(!dir_one.empty()){
           inint_prm.include_prm.min_file_size = stoi(dir_one);
    }
    dir_one="";
    std::cout << "Маски имен файлов разрешенных для сравнения (не зависят от регистра):" << std::endl;
    while (getline(cin, dir_one)) {
        if(dir_one=="")break;
        inint_prm.include_prm.mask_name_files.push_back(maskToRegex(dir_one));
    }

    dir_one="";
    std::cout << "Размер блока, которым производится чтения файлов:" << std::endl;
    getline(cin, dir_one);if(!dir_one.empty()){
           inint_prm.compare_prm.size_blok = stoi(dir_one);
    }
    std::cout << "Введите тип ключа md5 или crc2 (по умолчанию crc32):" << std::endl;
    getline(cin, dir_one);
    inint_prm.hesh = dir_one;
    return inint_prm;
}


void init_pole_prm(auto& prm,string name,po::variables_map& vm){
    if (vm.count(name)) {
         prm = vm[name].as<std::remove_reference_t<decltype(prm)>>();
     }
}

Tinint_prm start_wit_argc(int argc, char ** argv){
    Tinint_prm inint_prm;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Просмотр справки")
        ("scan_directories,sd", po::value<vector<string>>()->multitoken(), "Директории для сканирования")
        ("except_directories,exd", po::value<vector<string>>()->multitoken(), "Директории для исключения из сканирования")
        ("scan_level,sl",po::value<int>()->default_value(inint_prm.include_prm.scan_level), "Уровень сканирования (один на все директории, 0 - только указанная директория без вложенных)")
        ("min_file_size,min",po::value<int>()->default_value(inint_prm.include_prm.min_file_size), "Минимальный размер файла, по умолчанию проверяются все файлы больше 1 байта.")
        ("mask_name_files,m", po::value<vector<string>>()->multitoken(), "Маски имен файлов разрешенных для сравнения (не зависят от регистра ?- один символ, * - любое колличество символов)")
        ("size_blok,szb",po::value<int>()->default_value(inint_prm.compare_prm.size_blok), "Размер блока, которым производится чтения файлов")
        ("hesh,hs", po::value<string>()->default_value(inint_prm.hesh), "Один из имеющихся алгоритмов хэширования (crc32, md5)")
            ;


    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
       cout << desc << endl;
    }

    init_pole_prm(inint_prm.scan_directories,"scan_directories",vm);
    init_pole_prm(inint_prm.except_directories,"except_directories",vm);
    init_pole_prm(inint_prm.include_prm.scan_level,"scan_level",vm);
    init_pole_prm(inint_prm.include_prm.min_file_size,"min_file_size",vm);
    vector<string> mask_name_files;
    init_pole_prm(mask_name_files,"mask_name_files",vm);
    for(auto mask_str:mask_name_files){
        inint_prm.include_prm.mask_name_files.push_back(maskToRegex(mask_str));
    }

    init_pole_prm(inint_prm.compare_prm.size_blok,"size_blok",vm);
    init_pole_prm(inint_prm.hesh,"hesh",vm);

    return inint_prm;
}

#if 1
 int main (int argc, char ** argv) {
    Tdirectori::compare_files_dirs((argc>1?start_wit_argc(argc,argv):start_without_argc()).creator_Tdirectori());
    return 0;
}
#endif
