{ nixpkgs ? import <nixpkgs> {} }:

with nixpkgs;
flameshot.overrideAttrs (_: {
  src = lib.cleanSourceWith {
    filter = path: type: type != "directory" || (
      baseNameOf path != "build" && baseNameOf path != ".git"
    );
    src = ./.;
  };
})
