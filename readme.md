# Aseprite Loader

Aseprite Loader is a C++11 library that loads .ase files, the file format used in the pixel editor [Aseprite](https://aseprite.org).

## Usage
- Copy ase_loader.h and decompressor.h into your project folder
- Only include ase_loader.h:
```c++
#include "ase_loader.h"
```
- Available functions:
```c++
Ase_Output* Ase_Load(std::string path);
void Ase_Destroy_Output(Ase_Output* output);
```

## Example

```c++
#include <stdio.h>
#include "ase_loader.h"

int main() {

    Ase_Output* output = Ase_Load("example.ase");
    if (output) {
        std::cout << "Success!\n";
    }
    else {
        std::cout << "Failure.\n";
    }
    Ase_Destroy_Output(output);

    return 0;
}
```

## Contributing
Pull requests are welcome, but I do not plan or want this library to grow any bigger than it needs to be.
## License
[MIT](https://choosealicense.com/licenses/mit/)