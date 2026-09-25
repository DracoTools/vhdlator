-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A procedure's own constant as long as an element of the record signal it
-- is handed -- "constant C_NUM_USER_BITS_PER_WORD : natural :=
-- axistream_if.tuser'length" -- where a generic sizes that signal, and a
-- variable, a slice and a signal slice written against the constant. The
-- constant is a variable of the emitted procedure, so nothing written
-- against it can be a constant subtype: the array of words was emitted
-- with it in a packed range and refused by the back end, its words were
-- handed on as a structure declared once for the design that named it, a
-- two-level array whose own length is a number was never given its bounds,
-- and a slice of the signal's element was taken from the formal's open
-- subtype rather than the signal's. UVVM's axistream_expect and
-- axistream_transmit are the shape (bitvis_vip_axistream/src/
-- axistream_bfm_pkg.vhd).
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_sigwidthconst is
  generic (GC_USER_WIDTH : natural := 3);
end entity;

architecture a of t_vhdl_sigwidthconst is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_user_array is array (natural range <>) of std_logic_vector(7 downto 0);
  type t_if is record
    tdata : std_logic_vector;
    tuser : std_logic_vector;
    tvalid : std_logic;
  end record;
  signal axi : t_if(tdata(15 downto 0), tuser(GC_USER_WIDTH - 1 downto 0));

  function to_string(val : t_slv_array) return string is
    variable s : string(1 to val'length * (val(val'low)'length + 1));
    variable k : natural := 1;
  begin
    for i in val'range loop
      for j in val(i)'range loop
        s(k) := std_logic'image(val(i)(j))(2);
        k := k + 1;
      end loop;
      s(k) := ' ';
      k := k + 1;
    end loop;
    return s;
  end function;

  -- Words as wide as the signal's user element, built from the constant
  procedure expect(constant exp_user_array : t_user_array;
                   signal axistream_if : inout t_if; variable text : out string) is
    constant C_NUM_USER_BITS_PER_WORD : natural := axistream_if.tuser'length;
    variable v_rx : t_slv_array(exp_user_array'range)(C_NUM_USER_BITS_PER_WORD - 1 downto 0);
  begin
    assert v_rx(v_rx'low)'length = 3 severity failure;
    for w in v_rx'range loop
      v_rx(w) := exp_user_array(w)(C_NUM_USER_BITS_PER_WORD - 1 downto 0);
    end loop;
    text := to_string(v_rx);
  end procedure;

  -- The signal's element driven and read through a slice the constant ends
  procedure send(constant user_array : t_user_array; signal axistream_if : inout t_if;
                 variable seen : out std_logic_vector) is
    constant C_NUM_USER_BITS_PER_WORD : natural := axistream_if.tuser'length;
  begin
    for w in user_array'range loop
      axistream_if.tuser(C_NUM_USER_BITS_PER_WORD - 1 downto 0)
        <= user_array(w)(C_NUM_USER_BITS_PER_WORD - 1 downto 0);
      wait for 1 ns;
      seen(w) := axistream_if.tuser(C_NUM_USER_BITS_PER_WORD - 1 downto 0)(0);
    end loop;
  end procedure;
begin
  process
    variable ua : t_user_array(0 to 1) := (x"05", x"02");
    variable text : string(1 to 8);
    variable seen : std_logic_vector(0 to 1);
  begin
    expect(ua, axi, text);
    assert text = "101 010 " report text severity failure;
    send(ua, axi, seen);
    assert axi.tuser = "010" and seen = "10" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
