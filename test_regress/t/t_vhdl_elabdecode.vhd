-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A record built by an impure function from a string generic, whose width
-- elements are literals or come from a parameter that folds, while the rest
-- are read out of the string while the design runs. The widths size the
-- design's signals. The function takes a string, which the fold did not
-- hold, so none of the record was worked out and every signal sized from it
-- was refused as not constant. VUnit's verification component testbenches
-- decode their tb_cfg this way (vunit/vhdl/verification_components/test/
-- tb_avalon_master.vhd). Behind that stood two more: an element read out of
-- the string handed to a component's generic was made a parameter the back
-- end could not fold, and an array type bounded by such an element, or by a
-- constant an impure call gives, was emitted as a fixed array with a range
-- that is not constant.
entity t_vhdl_elabdecode_sub is
  generic (prob : real);
  port (seen : out real);
end entity;

architecture a of t_vhdl_elabdecode_sub is
begin
  seen <= prob * 2.0;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_elabdecode is
  generic (encoded : string := "transfers:5");
end entity;

architecture a of t_vhdl_elabdecode is
  type cfg_t is record
    data_width : positive;
    addr_width : positive;
    prob : real;
    transfers : positive;
    rate : real;
  end record;

  impure function get(s : string; key : string) return string is
  begin
    for i in s'range loop
      if s(i) = ':' then
        return s(i + 1 to s'right);
      end if;
    end loop;
    return "";
  end;

  impure function decode(e : string) return cfg_t is
  begin
    return (data_width => 32,
            addr_width => 12,
            prob => 0.5,
            transfers => positive'value(get(e, "transfers")),
            rate => real'value(get(e, "rate")));
  end function;

  -- A width given by a parameter the call writes as a number
  impure function decode_w(e : string; w : positive) return cfg_t is
  begin
    return (data_width => w,
            addr_width => w / 2,
            prob => 0.25,
            transfers => positive'value(get(e, "transfers")) + 1,
            rate => 1.0);
  end function;

  constant cfg : cfg_t := decode(encoded);
  constant cfg2 : cfg_t := decode_w(encoded, 16);

  impure function count return positive is
  begin
    return positive'value(get(encoded, "transfers"));
  end;
  constant n : positive := count;
  signal seen : real := 0.0;

  signal data : std_logic_vector(cfg.data_width - 1 downto 0);
  signal addr : std_logic_vector(cfg.addr_width - 1 downto 0);
  signal be : std_logic_vector(cfg.data_width / 8 - 1 downto 0);
  signal data2 : std_logic_vector(cfg2.data_width - 1 downto 0);
  signal addr2 : std_logic_vector(0 to cfg2.addr_width - 1);
begin
  sub : entity work.t_vhdl_elabdecode_sub
    generic map (prob => cfg.rate)
    port map (seen => seen);

  process
    -- As long as only running says
    type arr_t is array (0 to cfg.transfers - 1) of integer;
    variable a : arr_t;
    type arr2_t is array (0 to n - 1) of integer;
    variable b : arr2_t;
    variable s : integer := 0;
  begin
    for i in a'range loop
      a(i) := i;
      b(i) := 2 * i;
    end loop;
    for i in a'range loop
      s := s + a(i) + b(i);
    end loop;
    assert a'length = 5 and b'length = 5 and s = 30 severity failure;
    assert data'length = 32 and addr'length = 12 and be'length = 4 severity failure;
    assert data2'length = 16 and addr2'length = 8 and addr2'ascending severity failure;
    -- What only the string says is still read from it while running
    assert cfg.transfers = 5 and cfg2.transfers = 6 severity failure;
    assert cfg.prob = 0.5 and cfg2.prob = 0.25 severity failure;
    data <= (others => '1');
    data2 <= x"a5c3";
    wait for 1 ns;
    assert data = x"ffffffff" and data2(3 downto 0) = x"3" severity failure;
    assert seen = 10.0 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
