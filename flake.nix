{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/release-24.11";

  outputs = { self, nixpkgs }: {
    devShells.x86_64-linux.default =
      let pkgs = nixpkgs.legacyPackages.x86_64-linux; in import ./shell.nix { inherit pkgs; };

    packages.x86_64-linux.default =
      let pkgs = nixpkgs.legacyPackages.x86_64-linux; in
      pkgs.stdenv.mkDerivation {
        pname = "drm-presentation";
	version = "1.0.0";
	src = ./.;

        nativeBuildInputs = with pkgs; [ meson ninja pkg-config cmake libdrm systemdLibs ];
      };
  };
}
