{
  description = "Convert your files";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }: {

    packages.x86_64-linux.default = 
      with import nixpkgs { system = "x86_64-linux"; };
      stdenv.mkDerivation {
      	nativeBuildInputs = [
	  # raylib
	  gnumake
	  pkg-config
	  blueprint-compiler
	];
	buildInputs = [
	  gtk4
	  gtkmm4
	];
        name = "dizelge";
        src = self;
        buildPhase = "make";
	installPhase = "make install prefix=$out";
      };
  };
}
