-- DESCRIPTION: Verilator: VHDL method of a shared variable called through an
-- alias of an external name
--
-- "alias ram is << variable u1.ram : ram_t >>; ram.set(0, v);" calls a
-- method of the protected object an instance declares (8.7, 5.6.2): the
-- instance sees what the call stored, and a function method answers from it
-- (GHDL gna issue24).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_extprot_pkg is
  type ram_t is protected
    procedure set(constant index : in integer; variable v : in integer);
    impure function get(index : integer) return integer;
  end protected ram_t;
end package;

package body t_vhdl_extprot_pkg is
  type ram_t is protected body
    type memory_array_t is array (natural range <>) of integer;
    variable mem : memory_array_t(0 to 15) := (others => 0);
    procedure set(constant index : in integer; variable v : in integer) is
    begin
      mem(index) := v;
    end procedure;
    impure function get(index : integer) return integer is
    begin
      return mem(index);
    end function;
  end protected body ram_t;
end package body;

use work.t_vhdl_extprot_pkg.all;

entity t_vhdl_extprot_sub is
  port (done : out boolean := false);
end entity;

architecture a of t_vhdl_extprot_sub is
  shared variable ram : ram_t;
begin
  process is
    variable seven : integer := 7;
  begin
    wait for 1 ns;
    -- What the other process stored through the external name
    assert ram.get(3) = 42 severity failure;
    assert ram.get(4) = 0 severity failure;
    ram.set(4, seven);
    done <= true;
    wait;
  end process;
end architecture;

use work.t_vhdl_extprot_pkg.all;

entity t_vhdl_extprot is
end entity;

architecture a of t_vhdl_extprot is
  signal done : boolean;
begin
  u1 : entity work.t_vhdl_extprot_sub port map (done);

  process is
    alias ram is << variable u1.ram : ram_t >>;
    variable v : integer := 42;
  begin
    ram.set(3, v);
    assert ram.get(3) = 42 severity failure;
    wait until done;
    assert ram.get(4) = 7 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
