{
  description = "A D compiler written in C++";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in {
      formatter = forAllSystems (pkgs: pkgs.writeShellApplication {
        name = "dlang-format";
        runtimeInputs = [ pkgs.clang-tools ];
        text = ''
          find src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 | xargs -0 -r clang-format -i
        '';
      });
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [ clang llvm meson ninja pkg-config git ];
        };
      });
    };
}
