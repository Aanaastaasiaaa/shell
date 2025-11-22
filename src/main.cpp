#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib> // для getenv()

using namespace std;

int main()
{
    vector<string> history;
    string input;
    bool running = true;
<<<<<<< HEAD
 string history_file = "kubsh_history.txt";
ofstream write_file(history_file, ios::app);
=======
    string history_file = "kubsh_history.txt";
    
    // Открываем файл для записи в режиме добавления
    ofstream write_file(history_file, ios::app);
    
    if (!write_file.is_open()) {
        cerr << "Error: Could not open history file!" << endl;
        return 1;
    }

>>>>>>> 2b20894db7b5114fd087c8f5ddd1ba8579a7ad28
    while (running && getline(cin, input)) 
    {
        if (input.empty()) {
            continue;
        }
<<<<<<< HEAD
        write_file << input << endl;
        write_file.flush(); 
=======
        
        // Записываем команду в историю файла
        write_file << input << endl;
        write_file.flush();
        
        // Добавляем команду в историю памяти
        history.push_back(input);
        
>>>>>>> 2b20894db7b5114fd087c8f5ddd1ba8579a7ad28
        if (input == "\\q") 
        {
            running = false;
            break;
        }
<<<<<<< HEAD
       else if (input.find("debug ") == 0) 
        {
=======
        else if (input.find("debug ") == 0) {
>>>>>>> 2b20894db7b5114fd087c8f5ddd1ba8579a7ad28
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
        }
        else if (input.substr(0,4) == "\\e $") {
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
        else
        {
            cout << input << ": command not found" << endl;
        }
 write_file.close();
    }
    
    write_file.close();
    return 0;
}


