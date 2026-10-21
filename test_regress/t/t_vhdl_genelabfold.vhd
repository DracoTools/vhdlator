-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A generate's condition is static at elaboration (11.8): PoC's "VENDOR /=
-- VENDOR_XILINX or N < 6", VENDOR a function with no arguments, so written as a
-- name, that looks the board up in a table of records holding an array of
-- records. Left as a call, the back end could not work it out ("can't determine
-- constant"). The call is now worked out at elaboration, the generic left to
-- the instance.

package t_vhdl_genelabfold_pkg is
  type eth_t is record
    Style : string(1 to 8);
    Addr : natural;
  end record;
  type eth_vector is array (natural range <>) of eth_t;
  type info_t is record
    Name : string(1 to 16);
    Device : string(1 to 16);
    Eth : eth_vector(0 to 1);
    EthCount : natural;
  end record;
  type info_vector is array (natural range <>) of info_t;
  constant C_LIST : info_vector := (
    (Name => "KC705           ", Device => "XC7K325T        ",
     Eth => (("RGMII   ", 7), ("SGMII   ", 2)), EthCount => 2),
    (Name => "GENERIC         ", Device => "GENERIC         ",
     Eth => (("NONE    ", 0), ("NONE    ", 0)), EthCount => 0));
  function board(n : string) return natural;
  function vendor return natural;
end package;
package body t_vhdl_genelabfold_pkg is
  function board(n : string) return natural is
  begin
    for i in C_LIST'range loop
      if C_LIST(i).Name(1 to n'length) = n then return i; end if;
    end loop;
    report "unknown board " & n severity failure;
    return C_LIST'high;
  end function;
  function vendor return natural is
    constant DEV : string(1 to 16) := C_LIST(board("GENERIC")).Device;
  begin
    if DEV(1 to 2) = "XC" then return 1; end if;
    return 0;
  end function;
end package body;

use work.t_vhdl_genelabfold_pkg.all;
entity t_vhdl_genelabfold_sub is
  generic (N : natural);
end;
architecture a of t_vhdl_genelabfold_sub is
begin
  g1 : if vendor /= 1 or N < 6 generate
    process begin
      report "sub" & integer'image(N) & " generic";
      wait;
    end process;
  end generate;
  g2 : if vendor = 1 and N >= 6 generate
    process begin
      report "sub" & integer'image(N) & " xc" severity failure;
      wait;
    end process;
  end generate;
end;

use work.t_vhdl_genelabfold_pkg.all;
entity t_vhdl_genelabfold is end;
architecture t of t_vhdl_genelabfold is
begin
  s1 : entity work.t_vhdl_genelabfold_sub generic map (N => 3);
  s2 : entity work.t_vhdl_genelabfold_sub generic map (N => 9);
  g0 : if vendor = 0 generate
    process begin
      wait for 1 ns;
      report "*-* All Finished *-*";
      wait;
    end process;
  end generate;
end;
