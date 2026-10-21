-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A procedure of a package hands its out and inout parameters to a method
-- of a protected type. Where the address's width comes from the instance's
-- port, the method is called in its general form, whose data parameter
-- carries its own bounds, and the byte-sized actual has to be copied back
-- into after the call (OSVVM's MemRead from the Axi4Lite memory model)

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_protdynout_pkg is
  type rec_t is record
    addr : std_logic_vector;
  end record;
  procedure rd(a : in std_logic_vector; d : out std_logic_vector);
  procedure wr(a : in std_logic_vector; d : in std_logic_vector);
  procedure flip(a : in std_logic_vector; d : inout std_logic_vector);
end package;

package body t_vhdl_protdynout_pkg is
  type store_t is protected
    procedure get(a : std_logic_vector; d : out std_logic_vector);
    procedure put(a : std_logic_vector; d : std_logic_vector);
    procedure inv(a : std_logic_vector; d : inout std_logic_vector);
  end protected;
  type store_t is protected body
    variable mem : std_logic_vector(7 downto 0) := "UUUUUUUU";
    procedure get(a : std_logic_vector; d : out std_logic_vector) is
    begin
      d := mem;
    end procedure;
    procedure put(a : std_logic_vector; d : std_logic_vector) is
    begin
      mem := d;
    end procedure;
    procedure inv(a : std_logic_vector; d : inout std_logic_vector) is
    begin
      d := not d;
    end procedure;
  end protected body;
  shared variable st : store_t;
  procedure rd(a : in std_logic_vector; d : out std_logic_vector) is
  begin
    st.get(a, d);
  end procedure;
  procedure wr(a : in std_logic_vector; d : in std_logic_vector) is
  begin
    st.put(a, d);
  end procedure;
  procedure flip(a : in std_logic_vector; d : inout std_logic_vector) is
  begin
    st.inv(a, d);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_protdynout_pkg.all;
entity t_vhdl_protdynout_m is
  port (b : inout rec_t);
end entity;
architecture a of t_vhdl_protdynout_m is
begin
  process
    variable addr : std_logic_vector(b.addr'range);
    variable bd : std_logic_vector(7 downto 0);
    variable word : std_logic_vector(31 downto 0) := (others => '0');
  begin
    addr := (others => '0');
    wr(addr, "10100101");
    wait for 1 ns;
    rd(addr, bd);
    assert to_string(bd) = "10100101" report "rd gave " & to_string(bd) severity failure;
    flip(addr, bd);
    assert to_string(bd) = "01011010" report "flip gave " & to_string(bd) severity failure;
    -- Into a slice of a wider object, as the memory model fills its word
    rd(addr, word(15 downto 8));
    assert to_string(word) = "00000000000000001010010100000000"
      report "slice rd gave " & to_string(word) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_protdynout_pkg.all;
entity t_vhdl_protdynout is
end entity;
architecture a of t_vhdl_protdynout is
  signal s : rec_t(addr(31 downto 0));
begin
  u : entity work.t_vhdl_protdynout_m port map (b => s);
end architecture;
