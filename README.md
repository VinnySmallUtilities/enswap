# enswap
For Linux; require to install [.NET 7.0](https://dotnet.microsoft.com/download)

swapfile expander linux service. If Linux has less than 1.5 GB available memory, it allocate new swap

## build:

    git clone https://github.com/VinnySmallUtilities/enswap
    cd enswap
    dotnet publish -c Release --self-contained false /p:PublishSingleFile=true --output ./build/


## Executing
Let's correct the "enswap.service" file and "options" file


first executing:
    enswap options


let's see the created "options" file and correct it
* first string:  parameter for file length for single swap file (format for "fallocate" program)
* second string: parameter for available memory (in Mb)
* third string:  a file path template for swap files
* fourth string: sdel utility path (https://github.com/VinnySmallUtilities/sdel)
* 5: interval for the memory control cycle. In ms


second executing:

    sudo cp -v enswap.service /lib/systemd/system/
    sudo systemctl enable enswap.service
    sudo systemctl daemon-reload
    sudo systemctl start enswap.service

