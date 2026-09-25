-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A copy of a procedure made for one call, whose "inout" parameter the
-- actual's width alone sizes -- "tmp : std_logic_vector(writedata'range)"
-- -- hands that parameter on to a procedure that holds it with its own
-- bounds, because what the second call is given for another parameter has
-- a length only running decides. The fixed value was handed over as it
-- stood where a carrier was wanted, and the back end stopped on an
-- internal error. VUnit's bus_master_pkg read_bus with a natural address
-- is the shape (vunit/vhdl/verification_components/src/
-- bus_master_pkg-body.vhd).
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_inoutcarry is
end entity;

architecture a of t_vhdl_inoutcarry is
  type handle_t is record
    alen : positive;
  end record;

  constant width : positive := 32;
  signal net : std_logic := '0';
  signal writedata : std_logic_vector(width - 1 downto 0);
  signal upword : std_logic_vector(0 to 11);

  function to_address(h : handle_t; address : natural) return std_logic_vector is
  begin
    return std_logic_vector(to_unsigned(address, h.alen));
  end;

  -- Holds data with its own bounds: the address has a length only running
  -- decides, so no copy is made for the call
  procedure read_bus(signal net : inout std_logic;
                     constant address : std_logic_vector;
                     variable data : inout std_logic_vector) is
  begin
    data := (data'range => '0');
    data(data'left) := address(address'right);
    data(data'right) := '1';
  end procedure;

  procedure read_bus(signal net : inout std_logic;
                     constant h : handle_t;
                     constant address : natural;
                     variable data : inout std_logic_vector) is
    variable keep : std_logic_vector(data'range);
  begin
    keep := data;
    read_bus(net, to_address(h, address), data);
    data := data xor keep;
  end;
begin
  process
    variable tmp : std_logic_vector(writedata'range) := (30 => '1', others => '0');
    variable up : std_logic_vector(upword'range) := (1 => '1', others => '0');
  begin
    read_bus(net, (alen => 16), 1, tmp);
    assert tmp = x"c0000001" severity failure;
    tmp := (30 => '1', others => '0');
    read_bus(net, (alen => 8), 3, tmp);
    assert tmp = x"c0000001" severity failure;
    read_bus(net, (alen => 16), 1, up);
    assert up = "110000000001" severity failure;
    read_bus(net, (alen => 16), 2, up);
    assert up = "110000000000" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
