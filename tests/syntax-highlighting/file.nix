{
  pkgs,
  lib,
  config,
  inputs,
  ...
}:

let
  /*
  Modify the package for GNU Hello.
  Update to latest version and apply a custom patch.
  */
  hello = pkgs.hello.overrideAttrs (attrs: rec {
    version = "2.12.1";

    src = fetchurl {
      url = "mirror://gnu/hello/hello-${version}.tar.gz";
      hash = "sha256-jZkUKv2SV28wsM18tCqNxoCZmLxdYH2Idh9RLibH2yA=";
    };

    patches = attrs.patches or [] ++ [
      # Cherry pick a crash fix.
      ../pkgs/hello/fix-crash.patch
    ];
  });
in
{
  environment.systemPackages = with pkgs; [
    firefox
    hello
  ];

  home-manager.users.jtojnar = { lib, ... }: {
    dconf.settings = {
      "org/gnome/desktop/input-sources" = {
        sources = [
          (lib.hm.gvariant.mkTuple [
            "xkb"
            "${config.services.xserver.layout}${lib.optionalString (config.services.xserver.xkbVariant != "") "+" + config.services.xserver.xkbVariant}"
          ])
        ];
      };
    };

    home.file.".config/npm/npmrc".text = ''
      prefix=''${XDG_DATA_HOME}/npm
    '';
  };
}
