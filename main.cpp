#include <iostream>
#include <string>
using namespace std;
int main() {
  string line;
  while (true)
  {
    if(!getline(cin, line))
  {
    break;
  }
  cout<<line<<endl;
  }
  return 0;
}
