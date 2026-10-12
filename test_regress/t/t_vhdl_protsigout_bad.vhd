-- DESCRIPTION: Verilator: VHDL protected method driving a signal parameter, not carried
--
-- A method that drives its signal formal after a delay, and a call whose actual several drivers resolve, are refused with a message.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_protsigout_bad_pkg is
  type t is protected
    procedure late(signal a : out std_logic);
    procedure now(signal a : out std_logic);
  end protected;
end package;
package body t_vhdl_protsigout_bad_pkg is
  type t is protected body
    procedure late(signal a : out std_logic) is
    begin
      a <= '1' after 1 ns;
    end procedure;
    procedure now(signal a : out std_logic) is
    begin
      a <= '1';
    end procedure;
  end protected body;
end package body;
library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_protsigout_bad_pkg.all;
entity t_vhdl_protsigout_bad is end entity;
architecture a of t_vhdl_protsigout_bad is
  shared variable obj : t;
  signal c : std_logic := '0';
  signal r : std_logic := 'Z';
begin
  r <= 'L';
  process
  begin
    obj.late(c);
    obj.now(r);
    wait;
  end process;
end architecture;
