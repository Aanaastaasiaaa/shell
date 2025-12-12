#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>      
#include <cstring>     
#include <cerrno>     

using namespace std;

// Добавляем константы для задания 10:
#define SECTOR_SIZE 512
#define MBR_SIGNATURE_OFFSET 510

// Функция для проверки загрузочной сигнатуры диска (задание 10)
void check_bootable_disk(const char *disk) {
    char path[256];
    snprintf(path, sizeof(path), "/dev/%s", disk);

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror("Ошибка открытия диска");
        return;
    }

    unsigned char buffer[SECTOR_SIZE];
    if (fread(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE) {
        perror("Ошибка чтения сектора");
        fclose(file);
        return;
    }
    fclose(file);

    if (buffer[MBR_SIGNATURE_OFFSET] == 0x55 && buffer[MBR_SIGNATURE_OFFSET + 1] == 0xAA) {
        cout << "Диск " << disk << " является загрузочным (сигнатура 55AA)." << endl;
    } else {
        cout << "Диск " << disk << " не является загрузочным." << endl;
    }
}

int main() 
{
    vector<string> history;
    string input;
    bool running = true;
    string history_file = "kubsh_history.txt";
    ofstream write_file(history_file, ios::app);
    
    while (running && getline(cin, input)) 
    {
        if (input.empty()) {
            continue;
        }
        write_file << input << endl;
        write_file.flush();
        
        if (input == "\\q") 
        {
            running = false;
            break;
        }
        else if (input.find("debug ") == 0) 
        {
            string text = input.substr(6);
            
            if (text.size() >= 2) 
            {
                char first = text[0];
                char last = text[text.size()-1];
                if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) 
                {
                    text = text.substr(1, text.size()-2);
                }
            }
            
            cout << text << endl;
            history.push_back(input);
        }
        else if (input.substr(0,4) == "\\e $") 
        {
            string var_name = input.substr(4);
            const char* env_value = getenv(var_name.c_str());
            
            if (env_value != nullptr) {
                string value(env_value);
                if (value.find(':') != string::npos) {
                    stringstream ss(value);
                    string item;
                    while (getline(ss, item, ':')) {
                        cout << item << endl;
                    }
                } else {
                    cout << value << endl;
                }
            } else {
                cout << "Environment variable '" << var_name << "' not found" << endl;
            }
        }
        // ДОБАВЛЯЕМ ОБРАБОТКУ КОМАНДЫ \l (задание 10)
        else if (input.size() >= 3 && input.substr(0,3) == "\\l ") 
        {
            // Убираем "/dev/" если оно есть в начале
            string disk_name = input.substr(3);
            if (disk_name.find("/dev/") == 0) {
                disk_name = disk_name.substr(5);  // Убираем "/dev/"
            }
            
            if (!disk_name.empty()) {
                check_bootable_disk(disk_name.c_str());
            } else {
                cout << "Usage: \\l <disk_name> or \\l /dev/<disk_name>" << endl;
            }
            history.push_back(input);
        }
        else 
        {
            cout << input << ": command not found" << endl;
            history.push_back(input);
        }
    }
    
    write_file.close();
    return 0;
}
