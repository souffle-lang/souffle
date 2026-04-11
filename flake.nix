{
  inputs = {
    nixpkgs.url = "nixpkgs";
    utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, utils }:
    utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        souffle = pkgs.callPackage ./nix/package.nix {
          src = self;
          version = if self ? shortRev then "unstable-${self.shortRev}" else "unstable-dirty";
          inherit (pkgs.llvmPackages) openmp;
        };
      in
      {
        packages.default = souffle;
        packages.souffle = souffle;

        devShells.default = pkgs.mkShell {
          inputsFrom = [ souffle ];
        };
      }
    );
}
