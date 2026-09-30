-- DESCRIPTION: Verilator: VHDL handle built from a signal's 'length
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A record handle made by an impure function, from the 'length of a signal
-- whose bounds name a generic of the root, and handed to an instance whose
-- port widths are read out of it. An entity nobody instantiates hands one
-- of its own generics on the same way, and is not elaborated at all. A
-- procedure of the instance takes a signal constrained by the handle too.
-- The handle is made by a function that calls a procedure elaboration does
-- not follow, so its id is only known once the design runs, and the
-- generate condition comparing it with the null handle is decided by the
-- element that is known.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_elabsiglen_pkg is
  type handle_t is record
    id  : natural;
    len : natural;
  end record;
  constant null_handle : handle_t := (id => 0, len => 0);
  type counter_t is protected
    impure function bump return natural;
    procedure add(n : natural);
  end protected;
  impure function new_handle(len : natural) return handle_t;
  impure function data_length(h : handle_t) return natural;
end package;

package body t_vhdl_elabsiglen_pkg is
  type counter_t is protected body
    variable count : natural := 0;
    impure function bump return natural is
    begin
      count := count + 1;
      return count;
    end;
    procedure add(n : natural) is
    begin
      count := count + n;
    end;
  end protected body;

  shared variable counter : counter_t;

  -- Changes only what is shared, in a way elaboration does not follow: the
  -- handle's id is then not known before the design runs, and its length is
  procedure enroll(len : natural) is
  begin
    outer : for i in 0 to 3 loop
      for j in 0 to 3 loop
        exit outer when i + j = 2;
        counter.add(len);
      end loop;
    end loop;
  end;

  impure function new_handle(len : natural) return handle_t is
  begin
    enroll(len);
    return (id => counter.bump, len => len);
  end;

  impure function data_length(h : handle_t) return natural is
  begin
    return h.len;
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabsiglen_pkg.all;

entity t_vhdl_elabsiglen_child is
  generic (h : handle_t);
  port (
    d : in std_logic_vector(data_length(h) - 1 downto 0);
    q : out std_logic_vector(data_length(h) - 1 downto 0));
end entity;

architecture a of t_vhdl_elabsiglen_child is
begin
  process
    -- A signal formal is constrained by the handle as well
    procedure drive(signal s : out std_logic_vector(data_length(h) - 1 downto 0);
                    v : std_logic_vector) is
    begin
      s <= not v;
    end;
  begin
    q <= (others => '0');
    wait for 1 ns;
    assert d'length = 12 report "child width " & integer'image(d'length) severity failure;
    assert d = x"a5c" report "child data" severity failure;
    drive(q, d);
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabsiglen_pkg.all;

-- Instantiated by nothing
entity t_vhdl_elabsiglen_unused is
  generic (w : handle_t);
  port (d : in std_logic_vector(data_length(w) - 1 downto 0));
end entity;

architecture a of t_vhdl_elabsiglen_unused is
begin
  g : if w /= null_handle generate
    c : entity work.t_vhdl_elabsiglen_child
      generic map (h => w)
      port map (d => d, q => open);
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabsiglen_pkg.all;

entity t_vhdl_elabsiglen is
  generic (dl : natural := 12);
end entity;

architecture a of t_vhdl_elabsiglen is
  signal tdata : std_logic_vector(dl - 1 downto 0) := (others => '0');
  signal back  : std_logic_vector(dl - 1 downto 0);
  constant h : handle_t := new_handle(len => tdata'length);
begin
  g : if h /= null_handle generate
    c : entity work.t_vhdl_elabsiglen_child
      generic map (h => h)
      port map (d => tdata, q => back);
  end generate;

  process
    -- Sized by elaboration, so a length that does not match is only an
    -- error if the assignment runs
    variable v : std_logic_vector(data_length(h) - 1 downto 0);
    variable w : std_logic_vector(v'range);
  begin
    if v'length = 8 then
      v := x"ab";
      w := x"cd";
    end if;
    assert v'length = 12 and w'length = 12 report "variable width" severity failure;
    assert h.id = 25 report "handle id " & integer'image(h.id) severity failure;
    tdata <= x"a5c";
    wait for 2 ns;
    assert back = x"5a3" report "driven back" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
