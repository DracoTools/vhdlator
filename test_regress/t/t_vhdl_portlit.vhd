-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package sigs is
  -- An enumeration whose literals are named like the ports below
  type sig_t is (tdata, tlast, tkeep);
  function to_string(s : sig_t) return string;
end package;

package body sigs is
  function to_string(s : sig_t) return string is
  begin
    case s is
      when tdata => return "tdata";
      when tlast => return "tlast";
      when others => return "tkeep";
    end case;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.sigs.all;

entity mon is
  port (tlast : in std_logic);
end entity;

architecture a of mon is
begin
  -- The port is declared in the entity, whose declarative region this
  -- architecture continues, so it hides the literal the use clause brought
  -- in: this is to_string(std_ulogic), not to_string(sig_t)
  check : process(tlast)
  begin
    assert to_string(tlast) = "1" or to_string(tlast) = "0"
      report "read the literal, not the port" severity failure;
  end process;
end architecture;

-- A generic package, so that the entity below can declare an instance of it
package boxed is
  generic (type t; function widen(x : t) return integer is <>);
  function twice(x : t) return integer;
end package;

package body boxed is
  function twice(x : t) return integer is
  begin
    return 2 * widen(x);
  end function;
end package body;

package intwiden is
  function widen(x : integer) return integer;
end package;

package body intwiden is
  function widen(x : integer) return integer is
  begin
    return x;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.intwiden.all;

entity t_vhdl_portlit is
  -- A package the entity declares, named by the architecture below
  package int_boxed is new work.boxed generic map (t => integer, widen => widen);
end entity;

architecture a of t_vhdl_portlit is
  use int_boxed.all;
  signal s : std_logic := '0';
begin
  dut : entity work.mon port map (tlast => s);

  main : process
  begin
    s <= '1';
    wait for 1 ns;
    assert s = '1' report "s" severity failure;
    assert twice(4) = 8 report "twice" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
