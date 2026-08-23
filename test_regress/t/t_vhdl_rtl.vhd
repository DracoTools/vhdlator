-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Register transfer level VHDL, using only the predefined environment so the
-- test needs no library beyond STD.STANDARD. Everything here has to lower
-- into Verilator's own AST and simulate.

entity adder is
  generic (WIDTH : integer := 8);
  port (a, b : in  bit_vector(7 downto 0);
        s    : out bit_vector(7 downto 0));
end entity;

architecture rtl of adder is
begin
  s <= a xor b;
end architecture;

entity t_vhdl_rtl is
  generic (STAGES : integer := 3);
  port (clk   : in  bit;
        rst   : in  bit;
        en    : in  bit;
        sel   : in  bit_vector(1 downto 0);
        count : out bit_vector(7 downto 0);
        shift : out bit_vector(7 downto 0);
        muxed : out bit_vector(7 downto 0);
        pari  : out bit;
        summed : out bit_vector(7 downto 0));
end entity;

architecture rtl of t_vhdl_rtl is
  type state_t is (idle, running, done);
  type byte_arr is array (0 to 2) of bit_vector(7 downto 0);

  component adder is
    generic (WIDTH : integer := 8);
    port (a, b : in bit_vector(7 downto 0); s : out bit_vector(7 downto 0));
  end component;

  constant seed_c : bit_vector(7 downto 0) := "00000001";

  signal c     : bit_vector(7 downto 0);
  signal sh    : bit_vector(7 downto 0);
  signal state : state_t;
  signal pipe  : byte_arr;
  signal sumv  : bit_vector(7 downto 0);

  -- A function, with a variable and a loop of its own
  function parity_of (v : bit_vector(7 downto 0)) return bit is
    variable p : bit := '0';
  begin
    for i in 0 to 7 loop
      p := p xor v(i);
    end loop;
    return p;
  end function;

begin

  -- A clocked process with an asynchronous reset
  counter : process (clk, rst) is
  begin
    if rst = '1' then
      c <= (others => '0');
      state <= idle;
    elsif clk'event and clk = '1' then
      if en = '1' then
        c <= c xor seed_c;
        case state is
          when idle => state <= running;
          when running => state <= done;
          when done => state <= idle;
        end case;
      end if;
    end if;
  end process;

  -- A clocked process written with rising_edge's other spelling
  shifter : process (clk, rst) is
    variable tmp : bit_vector(7 downto 0);
  begin
    if rst = '1' then
      sh <= seed_c;
    elsif clk'event and clk = '1' then
      tmp := sh(6 downto 0) & sh(7);
      sh <= tmp;
    end if;
  end process;

  -- A for-generate, each iteration a clocked process of its own
  first : process (clk, rst) is
  begin
    if rst = '1' then
      pipe(0) <= (others => '0');
    elsif clk'event and clk = '1' then
      pipe(0) <= c;
    end if;
  end process;

  pipeline : for i in 1 to 2 generate
    stage : process (clk, rst) is
    begin
      if rst = '1' then
        pipe(i) <= (others => '0');
      elsif clk'event and clk = '1' then
        pipe(i) <= pipe(i - 1);
      end if;
    end process;
  end generate;

  -- An if-generate, chosen by a generic
  wide : if STAGES > 2 generate
    u_add : adder generic map (WIDTH => 8) port map (a => pipe(2), b => sh, s => sumv);
  else generate
    sumv <= (others => '0');
  end generate;

  -- A selected concurrent assignment
  with sel select
    muxed <= c        when "00",
             sh       when "01",
             pipe(2)  when "10",
             seed_c   when others;

  -- A conditional concurrent assignment
  shift <= sh when state = running else (others => '0');

  count  <= c;
  summed <= sumv;
  pari   <= parity_of(c);

end architecture;
