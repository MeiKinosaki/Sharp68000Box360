# Sharp68000Box360

Projeto experimental de emulador do **Sharp X68000 para Xbox 360 (RGH/JTAG)**, usando libXenon e compilação na nuvem pelo GitHub Actions.

> **Estado real:** a versão atual é um protótipo de inicialização e integração inicial. Ainda não é compatível com jogos comerciais nem implementa o hardware completo do X68000.

## O que existe nesta etapa

- Fluxo de compilação na nuvem com a imagem `free60/libxenon`.
- Geração e publicação de artefatos ELF quando a execução conclui com sucesso.
- Núcleo Motorola 68000 experimental com uma pequena fatia de instruções: NOP, RTS, MOVEQ, BRA/Bcc, BSR, JMP absoluto e STOP.
- Barramento experimental com 1 MiB de RAM, janela de ROM de 256 KiB e contadores de acessos não mapeados/escritas ignoradas.
- Scheduler inicial por orçamento de ciclos por quadro.
- Testes host para barramento, reset, instruções básicas, BSR/RTS e scheduler.

## O que ainda falta

- Conjunto completo de instruções e exceções do Motorola 68000.
- Mapa de memória real do X68000, MMIO e controladores.
- Carga de IPL/ROMs e mídia de jogos.
- Vídeo, áudio, teclado, armazenamento, temporização de dispositivos e interrupções.
- Validação de desempenho/compatibilidade no Xbox 360 real.
- Conversão e validação de `default.xex`.

## Compilar na nuvem

1. Abra [GitHub Actions](https://github.com/MeiKinosaki/Sharp68000Box360/actions).
2. Selecione **Xenon toolchain cloud build**.
3. Execute a branch `cloud/xenon-toolchain` ou aguarde a execução automática após um commit.
4. Abra a execução e baixe o artefato `sharp68000box360-xenon-cloud-build`.

A compilação bem-sucedida de um ELF valida apenas o bootstrap atual; não comprova compatibilidade com jogos.

## Branch de desenvolvimento

- `cloud/xenon-toolchain`
- [Pull request](https://github.com/MeiKinosaki/Sharp68000Box360/pull/1)

## Licença

Consulte o arquivo [LICENSE](LICENSE).
