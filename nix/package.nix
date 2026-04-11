{
  lib,
  gccStdenv,
  cmake,
  bison,
  flex,
  mcpp,
  makeWrapper,
  which,
  python3,
  ncurses,
  zlib,
  sqlite,
  libffi,
  libtool,
  llvmPackages,
  openmp ? null,
  src,
  version,
  enableOpenMP ? !gccStdenv.isDarwin,
  enable64BitDomain ? gccStdenv.isDarwin,
  enableDebug ? false,
}:

let
  stdenv = gccStdenv;
  toolsPath = lib.makeBinPath [ mcpp stdenv.cc python3 ];
  souffleCompileIncludes =
    "-I${ncurses.dev}/include -I${zlib.dev}/include -I${sqlite.dev}/include -I${libffi.dev}/include"
    + lib.optionalString (stdenv.cc.isClang && enableOpenMP) " -I${openmp}/include";
  souffleCompileCxxFlags =
    lib.optionalString (stdenv.isDarwin && stdenv.hostPlatform ? darwinMinVersion)
      "-mmacosx-version-min=${stdenv.hostPlatform.darwinMinVersion}";
  souffleCompileLdFlags =
    "-L${ncurses.out}/lib -L${zlib.out}/lib -L${sqlite.out}/lib -L${libffi.out}/lib"
    + lib.optionalString (stdenv.cc.isClang && enableOpenMP) " -L${openmp}/lib"
    + lib.optionalString stdenv.cc.isClang " -L${llvmPackages.libcxx}/lib";
in
stdenv.mkDerivation {
  pname = "souffle";
  inherit src version;

  enableParallelBuilding = true;

  nativeBuildInputs = [
    cmake
    bison
    flex
    mcpp
    makeWrapper
    libtool
    which
    python3.pkgs.wrapPython
  ];

  buildInputs =
    [
      ncurses
      zlib
      sqlite
      libffi
    ]
    ++ lib.optional (stdenv.cc.isClang && enableOpenMP) openmp;

  patches = lib.optional stdenv.isDarwin ./remove-lld-on-darwin.patch;

  cmakeFlags =
    [
      "-DSOUFFLE_GIT=OFF"
      "-DPACKAGE_VERSION=${version}"
      "-DSOUFFLE_BASH_COMPLETION=OFF"
      "-DSOUFFLE_ENABLE_TESTING=OFF"
    ]
    ++ lib.optional enableOpenMP "-DSOUFFLE_USE_OPENMP=ON"
    ++ lib.optional (!enableOpenMP) "-DSOUFFLE_USE_OPENMP=OFF"
    ++ lib.optional enable64BitDomain "-DSOUFFLE_DOMAIN_64BIT=ON"
    ++ lib.optional enableDebug "-DCMAKE_BUILD_TYPE=Debug";

  hardeningDisable = lib.optional enableDebug "all";
  dontStrip = enableDebug;

  postInstall = ''
    sed 's#"includes": "#"includes": "-I'$out'/include ${souffleCompileIncludes} #' -i $out/bin/souffle-compile.py
    sed 's#"cxx_flags": "#"cxx_flags": " ${souffleCompileCxxFlags} #' -i $out/bin/souffle-compile.py
    sed 's#"link_options": "#"link_options": "${souffleCompileLdFlags} #' -i $out/bin/souffle-compile.py

    wrapProgram "$out/bin/souffle" \
      --prefix PATH : "${toolsPath}"

    wrapProgram "$out/bin/souffle-compile.py" \
      --prefix PATH : "${toolsPath}"
  '';

  postFixup = ''
    wrapPythonPrograms
  '';

  meta = with lib; {
    description = "A translator of declarative Datalog programs into the C++ language";
    homepage = "https://souffle-lang.github.io/";
    platforms = platforms.unix;
    license = licenses.upl;
  };
}
