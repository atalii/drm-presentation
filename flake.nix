{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-24.11";

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

      
    packages.x86_64-linux.test-vm = self.nixosConfigurations.test-vm.config.system.build.vm;

    nixosConfigurations.test-vm = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
	{
          system.stateVersion = "24.11";
	  users.users.admin = {
	    isNormalUser = true;
	    password = "admin";
	    extraGroups = [ "wheel" ];
	  };

	  environment.systemPackages = [ self.packages.x86_64-linux.default ];
	}
      ];
    };
  };
}
