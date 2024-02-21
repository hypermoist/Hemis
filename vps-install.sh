#! /bin/bash
cd ~
echo "fetching latest Hemis version"
{
apt-get install unzip -y
.Hemis-cli stop
rm Hemis*
rm -rf ~/.Hemis-params
wget --quiet https://github.com/Hemis-Blockchain/Hemis/releases/latest/download/Hemis-Linux.zip && unzip Hemis-Linux.zip
mkdir -p ~/.Hemis-params && cd ~/.Hemis-params && wget https://github.com/Hemis-Blockchain/Hemis/releases/latest/download/Hemis-params.zip && unzip Hemis-params.zip && cd ~
} &> /dev/null
echo "Hemis succesfully installed"
exit