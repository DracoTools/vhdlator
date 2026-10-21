-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice whose ends move with a loop variable, "w_data(8*(i+1)-1 downto
-- 8*i)", of an element of a port whose record type leaves the element open:
-- handed to a function whose parameter is open, it is eight elements on
-- every iteration and keeps the bounds it was written with (8.5). The shape
-- of open-logic's AXI slave verification component.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_movslicerec_pkg is
  type ms_t is record
    w_data : std_logic_vector;
    w_strb : std_logic_vector;
  end record;
  function hexof(v : std_logic_vector) return string;
end package;
package body t_vhdl_movslicerec_pkg is
  function hexof(v : std_logic_vector) return string is
  begin
    return to_hstring(v) & "@" & integer'image(v'left) & ":" & integer'image(v'right);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_movslicerec_pkg.all;
entity t_vhdl_movslicerec_vc is
  port (ms : in ms_t);
end entity;
architecture a of t_vhdl_movslicerec_vc is
begin
  process
    variable seen : std.textio.line;
    variable n : natural := 0;
  begin
    wait for 1 ns;
    for i in 0 to ms.w_data'length / 8 - 1 loop
      if ms.w_strb(i) = '1' then
        std.textio.write(seen, hexof(ms.w_data(8 * (i + 1) - 1 downto 8 * i)) & " ");
        n := n + 1;
      end if;
    end loop;
    assert n = 3 report "count " & integer'image(n) severity failure;
    assert seen.all = "EF@7:0 BE@15:8 DE@31:24 " report "seen '" & seen.all & "'"
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_movslicerec_pkg.all;
entity t_vhdl_movslicerec is
end entity;
architecture a of t_vhdl_movslicerec is
  signal ms : ms_t(w_data(31 downto 0), w_strb(3 downto 0)) := (x"DEADBEEF", "1011");
begin
  u : entity work.t_vhdl_movslicerec_vc port map (ms => ms);
end architecture;
