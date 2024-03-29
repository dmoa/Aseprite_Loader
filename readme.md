# Aseprite Loader

Aseprite Loader is a C++11 library that loads .ase files, the file format used in the pixel editor [Aseprite](https://aseprite.org).

## Usage
- Copy ase_loader.h and decompressor.h into your project folder (or copy the single file under releases)
- Include ase_loader.h and define implementation:
```c++
#include "ase_loader.h"
#define ASE_LOADER_IMPLEMENTATION // define only in one file
```
- Available functions:
```c++
Ase_Output* Ase_Load(std::string path);
void Ase_Destroy_Output(Ase_Output* output);
void Ase_SetFlipVerticallyOnLoad(bool input_flag);
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